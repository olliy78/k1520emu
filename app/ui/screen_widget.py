"""
K1520 Emulator - Screen Display Widget (CRT / green phosphor)
=============================================================

GPU-rendered emulation of the A5120's green-phosphor CRT.

The K7024 core delivers a 640x288 byte framebuffer (one byte per pixel,
0x00 = off, 0xFF = on).  This widget uploads that framebuffer as an 8-bit
texture once per changed frame and renders it through a GLSL fragment shader
that reproduces the *look* of a real picture tube:

  - green-phosphor tint (on/off colours)
  - subtle barrel (tube) curvature + rounded corners
  - vignette and optional flicker
  - aperture-grille style mask

All the heavy per-pixel work happens on the GPU, so language/host overhead is
irrelevant: Python only uploads one texture and issues a handful of draw calls
per frame.  The C-ABI boundary is unchanged - the core still only emits raw
framebuffer bytes; all "make it look like a tube" logic lives here in the
frontend.

The display is deliberately stretched to a **4:3** aspect ratio (the visible
area of the real A5120 tube), letterboxed inside the widget, freely scalable,
and can go fullscreen (F11).

Tuning: every visual parameter lives in :class:`CRTParams` with in-code
defaults.  Edit the defaults (or assign to ``widget.params`` at runtime and the
next frame picks them up) to dial the look toward the real screen.
"""

from dataclasses import dataclass, field
from typing import Optional, Tuple

from PySide6.QtWidgets import QMenu, QVBoxLayout, QWidget
from PySide6.QtGui import QColor, QGuiApplication, QImage, QKeyEvent, QPainter, qRgb
from PySide6.QtCore import QRect, QRectF
from PySide6.QtCore import QEvent, Qt, QTimer, QSize, QElapsedTimer, Signal
from PySide6.QtOpenGLWidgets import QOpenGLWidget
from PySide6.QtOpenGL import (
    QOpenGLShader,
    QOpenGLShaderProgram,
    QOpenGLBuffer,
    QOpenGLVertexArrayObject,
    QOpenGLTexture,
)

from app.core_binding.k1520 import kern_sperre
from app.ui.einfuegen import TastenEinfueger
from app.ui.keyboard import qt_event_to_core_key

# ── Raw GL enum constants (avoid a PyOpenGL dependency) ───────────────────────
GL_COLOR_BUFFER_BIT = 0x00004000
GL_TRIANGLE_STRIP = 0x0005
GL_FLOAT = 0x1406
GL_TEXTURE0 = 0x84C0

# K7024 framebuffer geometry (pixels emitted by the core).
FB_WIDTH = 640
FB_HEIGHT = 288

# Displayed aspect ratio: the real A5120 tube is 4:3, so the 640x288 (20:9)
# framebuffer is intentionally stretched to fill a 4:3 area (letterboxed).
DISPLAY_ASPECT = 4.0 / 3.0

KOPIEREN_TEXT = "Bildschirminhalt als Text kopieren"
EINFUEGEN_TEXT = "Zwischenablage über Tastatur einfügen"
EINFUEGEN_ABBRECHEN_TEXT = "Einfügen abbrechen"

#: Zeichen, die auf der (US-)Tastatur mit Umschalt entstehen — fuer das synthetische Tastenereignis
#: beim Einfuegen; die Maschine bekommt das Zeichen selbst, Umschalt ist nur das Begleitbit.
_MIT_UMSCHALT = set('!"#$%&()*+:<>?@^_{|}~')


def hex_to_rgb(s: str) -> Tuple[float, float, float]:
    """'#RRGGBB' -> (r, g, b) in 0..1."""
    s = s.lstrip("#")
    return (int(s[0:2], 16) / 255.0,
            int(s[2:4], 16) / 255.0,
            int(s[4:6], 16) / 255.0)


def rgb_to_hex(rgb) -> str:
    """(r, g, b) in 0..1 -> '#RRGGBB'."""
    return "#{:02X}{:02X}{:02X}".format(
        *(max(0, min(255, int(round(c * 255)))) for c in rgb))


@dataclass
class CRTParams:
    """Tunable CRT look parameters (all read into shader uniforms each frame).

    Edit the defaults here to dial the emulation toward the real tube, or set
    ``screen_widget.params = CRTParams(...)`` / mutate fields at runtime.  A few
    fields are fixed (no GUI control) — change them here or via a saved config.
    """

    # Phosphor colours ("Farbe aktiv" / "Farbe inaktiv"), linear 0..1 RGB.
    phosphor_on: Tuple[float, float, float] = hex_to_rgb("#78d41d")   # lit pixel
    phosphor_off: Tuple[float, float, float] = hex_to_rgb("#050f05")  # dark pixel

    # Overall image (adjustable).
    brightness: float = 2.3
    contrast: float = 0.9

    # Aperture-grille style vertical mask.  A monochrome green tube has NO shadow
    # mask, so this is fixed off (no GUI control).
    mask_strength: float = 0.0
    mask_pitch: float = 1.0

    # Tube curvature (barrel distortion) amount per axis, and rounded corners
    # (adjustable).
    curvature: Tuple[float, float] = (0.16, 0.16)
    corner_radius: float = 0.1        # 0 = square

    # Edge darkening — fixed (no GUI control).
    vignette_strength: float = 0.35

    # Optional mains-hum flicker — fixed off (no GUI control).
    flicker_strength: float = 0.0

    # Raster geometry (like the CRT's H/V size + position controls): the image
    # can be made smaller than the tube face and shifted, so its content clears
    # the rounded corners.  scale 1.0 = fills the tube; offset in tube-width units.
    scale_x: float = 0.9
    scale_y: float = 0.9
    offset_x: float = 0.0
    offset_y: float = 0.0

    # Filterstufen einzeln abschaltbar (Checkboxen im CRT-Reiter).  Aus heisst:
    # die Stufe wird neutral geschaltet (Shader-Uniform), die Reglerwerte bleiben
    # erhalten.  (Kontrast, Kruemmung und Ecken-Rundung haben dafuer ihren Regler.)
    vignette_on: bool = True

    # Gesamtschalter: aus = schlichte Textkonsole (zwei Farben, ein Texturzugriff,
    # keine Mipmaps/Kruemmung/Vignette) — spart GPU, CPU und Speicher auf schwachen Rechnern.
    crt_on: bool = True

    # Bildrate der ANZEIGE in Hz, unabhaengig vom Tempo des Gastes (10x Tempo
    # braucht kein 500-Hz-Bild).
    bildrate: float = 30.0

    STUFEN = ("vignette_on", "crt_on")

    # ── (de)serialisation for config files ───────────────────────────────────

    def to_dict(self) -> dict:
        """Plain dict for YAML; colours as '#RRGGBB', geometry as lists."""
        return {
            "phosphor_on": rgb_to_hex(self.phosphor_on),
            "phosphor_off": rgb_to_hex(self.phosphor_off),
            "brightness": self.brightness,
            "contrast": self.contrast,
            "mask_strength": self.mask_strength,
            "mask_pitch": self.mask_pitch,
            "curvature": list(self.curvature),
            "corner_radius": self.corner_radius,
            "vignette_strength": self.vignette_strength,
            "flicker_strength": self.flicker_strength,
            "scale_x": self.scale_x,
            "scale_y": self.scale_y,
            "offset_x": self.offset_x,
            "offset_y": self.offset_y,
            "bildrate": self.bildrate,
            **{k: bool(getattr(self, k)) for k in self.STUFEN},
        }

    def update_from_dict(self, d: dict):
        """Apply values from a config dict in place (tolerant of missing keys)."""
        if not isinstance(d, dict):
            return
        for key in ("phosphor_on", "phosphor_off"):
            if key in d:
                v = d[key]
                setattr(self, key, hex_to_rgb(v) if isinstance(v, str) else tuple(v))
        if "curvature" in d and d["curvature"] is not None:
            self.curvature = tuple(d["curvature"])
        for key in ("brightness", "contrast",
                    "mask_strength", "mask_pitch", "corner_radius",
                    "vignette_strength", "flicker_strength", "scale_x",
                    "scale_y", "offset_x", "offset_y", "bildrate"):
            if key in d and d[key] is not None:
                setattr(self, key, float(d[key]))
        for key in self.STUFEN:
            if key in d and d[key] is not None:
                setattr(self, key, bool(d[key]))


# ── GLSL shaders (GLSL 1.20 - broadly compatible desktop profile) ─────────────

_VERT_SHADER = """
#version 120
attribute vec2 aPos;
attribute vec2 aUv;
varying vec2 vUv;
void main() {
    vUv = aUv;
    gl_Position = vec4(aPos, 0.0, 1.0);
}
"""

_FRAG_SHADER = """
#version 120
uniform sampler2D uScreen;
uniform vec2  uResolution;     // framebuffer size in texels (640, 288)
uniform vec3  uPhosphorOn;
uniform vec3  uPhosphorOff;
uniform float uBrightness;
uniform float uContrast;
uniform float uMask;
uniform float uMaskPitch;
uniform vec2  uCurvature;
uniform float uCorner;
uniform float uVignette;
uniform float uFlicker;
uniform float uTime;
uniform vec2  uScale;          // raster width/height (1.0 = fills the tube)
uniform vec2  uOffset;         // raster shift, in tube-width units
varying vec2 vUv;

const float PI = 3.14159265;

// Barrel-distort uv around the centre to fake the curved tube surface.
vec2 curve(vec2 uv) {
    vec2 c = uv * 2.0 - 1.0;            // -1..1
    vec2 off = abs(c.yx) * uCurvature;  // bend more toward the edges
    c += c * off * off;
    return c * 0.5 + 0.5;              // back to 0..1
}

// Sample the (binary) screen texture; 0 outside the raster rectangle.
// The fetch is kept in *uniform* control flow (clamp + multiply instead of an
// early return) so the screen-space derivatives — and thus automatic mipmap LOD
// selection — stay well-defined when the widget is minified.  Without mipmaps a
// downscaled tube drops thin glyph strokes (parts of letters "swallowed"); the
// trilinear mip chain averages them down instead.
// Mipmap-Stufe aus der UNGEKRÜMMTEN Abbildung (s. main): die Krümmung dehnt die
// Ableitungen zu den Ecken hin stark, die automatische Stufenwahl nähme dort
// eine viel zu grobe Stufe und mittelte das Raster zu einem hellen Grauschleier
// (helle Ränder/Ecken, ändert sich mit der Fenstergrösse).  Der Bias holt die
// Stufe auf den Wert zurück, den die ebene Abbildung hätte.
float gLodBias = 0.0;

float sampleOn(vec2 uv) {
    float inside = (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0)
                   ? 0.0 : 1.0;
    return texture2D(uScreen, clamp(uv, 0.0, 1.0), gLodBias).r * inside;
}

void main() {
    vec2 tube = curve(vUv);

    // Outside the (curved) tube surface -> black.
    if (tube.x < 0.0 || tube.x > 1.0 || tube.y < 0.0 || tube.y > 1.0) {
        gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);
        return;
    }

    // Map tube coordinates into the (scaled + shifted) raster coordinates.
    vec2 uv = (tube - 0.5 - uOffset) / uScale + 0.5;
    vec2 uvFlat = (vUv - 0.5 - uOffset) / uScale + 0.5;
    float gCurved = length(fwidth(uv * uResolution));
    float gFlat = length(fwidth(uvFlat * uResolution));
    gLodBias = min(log2(max(gFlat, 1e-4) / max(gCurved, 1e-4)), 0.0);
    float inR = (uv.x >= 0.0 && uv.x <= 1.0 && uv.y >= 0.0 && uv.y <= 1.0)
                ? 1.0 : 0.0;

    // How hard the brightness knob is pushed past the "normal" full-scale range.
    // 0 for brightness <= 2.0 (the well-behaved region), growing to 2.0 at the
    // slider max (4.0).  Above 2.0 the tube is "overdriven": whiter/blooming
    // pixels and eventually a faintly glowing raster (see below).  Everything
    // keyed off this is exactly 0 at 2.0, so the <=2.0 look is bit-identical.
    float drive = max(uBrightness - 2.0, 0.0);
    // 0 at brightness 2.0, ramps to 1.0 at the slider max (4.0).  Drives the
    // fade-out of the darkening filter (vignette) so cranking the
    // brightness removes the "grey veil" instead of adding to it.
    float od = clamp(drive * 0.5, 0.0, 1.0);
    // Fade for the darkening filters: 0 at brightness 2.0, fully 1.0 at 3.0, so
    // the vignette is completely gone from 3.0 upward.
    float veilFade = clamp(drive, 0.0, 1.0);
    float base = sampleOn(uv);
    float lit = base;

    // Brightness / contrast operate on the *intensity* (not RGB).  Up to full
    // scale the output stays on the phosphor off<->on line; the overdrive terms
    // below take over once the beam is driven past that point.
    // Contrast pivots around the "on" level: contrast=0 -> whole screen "on".
    float contrast = uContrast;   // Regler wirkt in jeder Fenstergrösse gleich
    lit = (lit - 1.0) * contrast + 1.0;
    float inten = lit * uBrightness;         // may exceed 1.0 (overdrive headroom)
    float litC = clamp(inten, 0.0, 1.0);

    vec3 col = mix(uPhosphorOff, uPhosphorOn, litC);

    // Overdrive #1 — white core: a harder-driven beam reads whiter.  Weighted by
    // litC so only lit pixels/halos gain white, never the dark background.
    float white = od * litC;
    col = mix(col, vec3(1.0), white * 0.5);

    // Overdrive #2 — ambient raster glow: past ~3.5 the whole tube (incl. unlit
    // cells) starts to glow *faintly* toward the phosphor colour, the way a real
    // CRT's brightness knob eventually makes the blanked raster visible.  Kept
    // low so the text/background contrast (readability) survives.
    float ambient = clamp((uBrightness - 3.5) / 0.5, 0.0, 1.0) * 0.18;
    col = max(col, uPhosphorOn * ambient);

    // Ausserhalb des Rasters (aber innerhalb der Röhre) leuchtet die Röhre wie ein
    // UNBELEUCHTETER Bildpunkt: derselbe Weg wie oben mit lit = 0, also je nach
    // Helligkeit/Kontrast der dunklere Farbton (nicht schwarz).
    float litB = clamp((1.0 - contrast) * uBrightness, 0.0, 1.0);
    vec3 bg = mix(uPhosphorOff, uPhosphorOn, litB);
    bg = mix(bg, vec3(1.0), od * litB * 0.5);
    bg = max(bg, uPhosphorOn * ambient);
    col = mix(bg, col, inR);

    // Aperture-grille style vertical mask (subtle for a monochrome tube).
    float mphase = fract(gl_FragCoord.x / uMaskPitch);
    float mask = mix(1.0, 0.75 + 0.25 * sin(mphase * 2.0 * PI), uMask);
    col *= mask;

    // Vignette (darken toward the edges of the tube face).  Faded out with
    // overdrive (gone from 3.0 up); and below brightness 1.0 it *increases*
    // continuously (up to 3x at brightness 0), dimming the tube from the edges in.
    vec2 d = tube - 0.5;
    float vigBoost = 1.0 + clamp(1.0 - uBrightness, 0.0, 1.0) * 2.0;
    float vig = 1.0 - (uVignette * vigBoost * (1.0 - veilFade)) * dot(d, d) * 4.0;
    col *= clamp(vig, 0.0, 1.0);

    // Rounded corners (mask out the tube corners).
    if (uCorner > 0.0) {
        vec2 q = abs(tube * 2.0 - 1.0);
        vec2 cd = max(q - (1.0 - uCorner), 0.0) / uCorner;
        float r = length(cd);
        col *= 1.0 - smoothstep(0.9, 1.0, r);
    }

    // Optional flicker.
    if (uFlicker > 0.0) {
        col *= 1.0 - uFlicker * (0.5 + 0.5 * sin(uTime * 120.0));
    }

    gl_FragColor = vec4(col, 1.0);
}
"""

# Schlichter Weg (CRT aus): Textur in die beiden Phosphorfarben abbilden, sonst nichts.
_FRAG_SHADER_EINFACH = """
#version 120
uniform sampler2D uScreen;
uniform vec3  uPhosphorOn;
uniform vec3  uPhosphorOff;
varying vec2 vUv;
void main() {
    gl_FragColor = vec4(mix(uPhosphorOff, uPhosphorOn, texture2D(uScreen, vUv).r), 1.0);
}
"""


def _set_anisotropy(tex, level: float = 16.0):
    """Anisotrope Filterung einschalten, wo die GL sie hergibt.

    Die Mipmap-Stufe wird aus der GRÖSSTEN Ableitung gewählt und halbiert dann
    BEIDE Achsen.  Im schmalen Fenster wird waagerecht verkleinert, senkrecht
    aber vergrössert — ohne Anisotropie verwischt die gewählte Stufe damit auch
    die Zeilen, die eigentlich scharf sein dürften.  Kein Fehler, wenn die
    Erweiterung fehlt: dann bleibt es beim trilinearen Verhalten von vorher.
    """
    try:
        tex.setMaximumAnisotropy(level)
    except (AttributeError, RuntimeError):
        pass


class ScreenWidget(QWidget):
    """Bildschirm-Widget fuer den K1520-Framebuffer.

    Zeichenflaeche wechselbar zur Laufzeit (``params.crt_on``): OpenGL mit CRT-Shader
    (:class:`_GlFlaeche`) oder schlichte CPU-Zeichnung (:class:`_QtFlaeche`), die auf
    Rechnern ohne brauchbare GPU deutlich billiger ist und keinen GL-Kontext haelt.
    """

    # Kept for backward compatibility with callers/tests referencing them.
    COLS = 80
    ROWS = 24
    #: Eingaberate von „Zwischenablage ueber Tastatur einfuegen" (Zeichen je Wirtssekunde).
    EINFUEGEN_ZEICHEN_PRO_S = 120
    WIDTH = FB_WIDTH
    HEIGHT = FB_HEIGHT

    # Fullscreen is owned by the main window (reparenting a QOpenGLWidget crashes
    # the GL context), so the widget only *requests* it via these signals.
    toggleFullscreenRequested = Signal()
    exitFullscreenRequested = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)

        self.emulator = None
        self.params = CRTParams()
        # Bildschirmtastatur, die Host-Tasten mitanzeigen darf (s. _map_key);
        # ohne sie geht die Eingabe unveraendert direkt an den Kern.
        self.key_sink = None

        # Power state: when off, the tube is dark and the framebuffer is not
        # polled (as if the machine had no power).
        self._powered = True

        # Latest framebuffer bytes waiting to be uploaded to the GL texture.
        self._fb_bytes: Optional[bytes] = None
        # Bildgroesse aus dem Kern (A5120/K8915/PRG: 640x288; PC 1715: 640x300 mit
        # Statuszeile bzw. 512x255 am K7221).  Aendert sie sich, wird die Textur
        # im GL-Kontext neu angelegt (paintGL).
        self._fb_w = FB_WIDTH
        self._fb_h = FB_HEIGHT
        self._texture_stale = False
        self._pending_upload = False

        # Die Zeichenflaeche (GL oder Qt) fuellt das Widget; sie wird in
        # parameter_geaendert() gewechselt, sobald params.crt_on es verlangt.
        self._layout = QVBoxLayout(self)
        self._layout.setContentsMargins(0, 0, 0, 0)
        self._flaeche = None
        self._flaeche_art = None
        self._flaeche_waehlen()

        # Poll the core for framebuffer changes at 50 Hz (like the hardware).
        self.timer = QTimer(self)
        self.timer.timeout.connect(self._on_update)
        self.timer.setInterval(self._bildintervall_ms())

        # Scalable: allow any size >= a sensible minimum, keep a 4:3 hint.
        self.setMinimumSize(320, 240)
        self.setFocusPolicy(Qt.StrongFocus)

        # Rechtsklick: Text kopieren / Zwischenablage ueber die Tastatur einfuegen.
        self._einfueger = TastenEinfueger(self.eingabe_druecken, self.eingabe_loslassen, self,
                                          rate=self.EINFUEGEN_ZEICHEN_PRO_S)
        self._einfueger.fertig.connect(self._einfuegen_fertig)
        self._eingabe_ereignis = None          # das gerade „gedrueckte“ Zeichen (fuer das Loslassen)
        self.setContextMenuPolicy(Qt.CustomContextMenu)
        self.customContextMenuRequested.connect(self._kontextmenue)

    # ── Public API (unchanged from the old widget) ───────────────────────────

    def set_emulator(self, emulator):
        """Connect to the emulator core binding."""
        self.emulator = emulator

    def _bildintervall_ms(self) -> int:
        return max(1, int(round(1000.0 / max(1.0, float(self.params.bildrate)))))

    def parameter_geaendert(self):
        """Nach einer Aenderung von :attr:`params`: Bildrate uebernehmen, Zeichenflaeche
        passend zu ``crt_on`` waehlen, neu zeichnen."""
        ms = self._bildintervall_ms()
        if self.timer.interval() != ms:
            self.timer.setInterval(ms)
        self._flaeche_waehlen()
        self._bild_neu()

    def _flaeche_waehlen(self):
        """Die Zeichenflaeche zu ``params.crt_on`` haben; die alte wird freigegeben (samt GL-Kontext)."""
        art = "gl" if self.params.crt_on else "qt"
        if art == self._flaeche_art:
            return
        alt, self._flaeche = self._flaeche, (_GlFlaeche(self) if art == "gl" else _QtFlaeche(self))
        self._flaeche_art = art
        self._layout.addWidget(self._flaeche)
        # Die neue Flaeche kennt das Bild noch nicht.
        self._texture_stale = True
        self._pending_upload = self._fb_bytes is not None
        if alt is not None:
            self._layout.removeWidget(alt)
            alt.hide()
            alt.setParent(None)        # der GL-Kontext raeumt sich ueber aboutToBeDestroyed ab
            alt.deleteLater()

    def _bild_neu(self):
        """Neu zeichnen lassen (die Flaeche zeichnet, nicht dieses Widget)."""
        if self._flaeche is not None:
            self._flaeche.update()

    def start_display(self):
        """Start polling the framebuffer and repainting."""
        self.timer.start()

    def stop_display(self):
        """Stop display updates."""
        self.timer.stop()

    def set_powered(self, on: bool):
        """Schaltet die Röhre ein/aus.  Aus = schwarzer Bildschirm, kein Polling."""
        self._powered = bool(on)
        if on:
            self.start_display()
        else:
            self.stop_display()
        self._bild_neu()   # sofort neu zeichnen (schwarz bzw. aktuelles Bild)

    def sizeHint(self) -> QSize:
        return QSize(640, 480)  # 4:3

    # ── Rechtsklick: Text kopieren, Zwischenablage einfuegen ───────────────────

    #: Ein Hinweis nach dem Einfuegen („N Zeichen, M uebersprungen"); das Hauptfenster zeigt ihn.
    einfuegenFertig = Signal(int, int)

    def kontextmenue_bauen(self) -> QMenu:
        menue = QMenu(self)
        a = menue.addAction(KOPIEREN_TEXT)
        a.setEnabled(self.emulator is not None)
        a.triggered.connect(lambda _c=False: self.text_kopieren())
        if self.einfuegen_laeuft():
            b = menue.addAction(EINFUEGEN_ABBRECHEN_TEXT)
            b.triggered.connect(lambda _c=False: self.einfuegen_abbrechen())
        else:
            b = menue.addAction(EINFUEGEN_TEXT)
            b.setEnabled(self.emulator is not None and self._powered
                         and bool(QGuiApplication.clipboard().text()))
            b.triggered.connect(lambda _c=False: self.einfuegen(QGuiApplication.clipboard().text()))
        return menue

    def _kontextmenue(self, pos):
        self.kontextmenue_bauen().exec(self.mapToGlobal(pos))

    def bildschirmtext(self) -> str:
        """Der Text des Bildes: Zeilen ohne Schlussleerzeichen, durch ``\n`` getrennt.

        Aus dem Bildwiederholspeicher der Karte (nicht aus dem gerenderten Bild).  Attribut- und
        Steuerzeichen werden zu Leerzeichen.  PC 1715: 25 Zeilen (mit Statuszeile), MC80: 25, sonst 24.
        """
        if self.emulator is None:
            return ""
        # PC 1715: Statuszeile; MC80: 25 EGOS-Textzeilen (aus dem Bitmap zurueckgewonnen).
        maschine = str(getattr(self.emulator, "machine", ""))
        roh = (self.emulator.screen_text(25) if maschine.startswith(("pc1715", "mc80"))
               else self.emulator.screen_text())
        zeilen = ["".join(c if " " <= c < "\x7f" else " " for c in z).rstrip()
                  for z in roh.split("\n")]
        return "\n".join(zeilen)

    def text_kopieren(self) -> str:
        """Den Bildschirminhalt als reinen Text in die Zwischenablage (und zurück)."""
        text = self.bildschirmtext()
        QGuiApplication.clipboard().setText(text)
        return text

    def einfuegen(self, text: str):
        """Text zeichenweise über die Tastatur eingeben (120 Zeichen/s, Zeichen ohne Taste entfallen)."""
        if self.emulator is None or not self._powered or not text:
            return
        self._einfueger.starten(text)

    def einfuegen_laeuft(self) -> bool:
        return self._einfueger.laeuft()

    def einfuegen_abbrechen(self):
        self._einfueger.abbrechen()

    def _einfuegen_fertig(self, eingegeben: int, uebersprungen: int):
        self.einfuegenFertig.emit(eingegeben, uebersprungen)

    @staticmethod
    def _zeichen_ereignis(c: str, art):
        """Das Wirtstasten-Ereignis, das *c* erzeugt hätte; ``None`` = keine Taste dafür."""
        if c == "\n":
            return QKeyEvent(art, Qt.Key_Return, Qt.NoModifier, "\r")
        if c == "\t":
            return QKeyEvent(art, Qt.Key_Tab, Qt.NoModifier, "\t")
        if not " " <= c <= "~":
            return None
        mods = Qt.ShiftModifier if (c.isupper() or c in _MIT_UMSCHALT) else Qt.NoModifier
        return QKeyEvent(art, ord(c.upper()), mods, c)    # Qt::Key_* = ASCII (Buchstaben gross)

    def eingabe_druecken(self, c: str) -> bool:
        """Ein Zeichen wie von der Wirtstastatur drücken — durch die Bildschirmtastatur, also mit
        derselben Abbildung wie echtes Tippen.  ``False``: keine Taste (übersprungen)."""
        if self.emulator is None:
            return False
        ev = self._zeichen_ereignis(c, QEvent.KeyPress)
        mapped = None if ev is None else self._map_key(ev, press=True)
        if mapped is None:
            if ev is not None and self.key_sink is not None:
                self._map_key(self._zeichen_ereignis(c, QEvent.KeyRelease), press=False)
            return False
        self.emulator.key_press(*mapped)
        self._eingabe_ereignis = self._zeichen_ereignis(c, QEvent.KeyRelease)
        return True

    def eingabe_loslassen(self):
        ev, self._eingabe_ereignis = self._eingabe_ereignis, None
        if ev is None or self.emulator is None:
            return
        mapped = self._map_key(ev, press=False)
        if mapped is not None:
            self.emulator.key_release(mapped[0])

    # ── Fullscreen (handled by the main window; see signals above) ────────────

    def focusNextPrevChild(self, next_widget: bool) -> bool:
        """Kein Fokuswechsel per Tabulator — TAB gehört dem emulierten Rechner.

        Qt wertet Tab/Shift-Tab normalerweise VOR :meth:`keyPressEvent` als
        Fokusweiterschaltung aus; die Taste käme dann nie beim K7637 an
        (``qt_event_to_core_key`` bildet ``Key_Tab`` bewusst ab).  ``False``
        heißt „nicht weitergeschaltet" — die Taste läuft normal weiter.
        """
        return False

    def keyPressEvent(self, event):
        key = event.key()
        # F11 toggelt Vollbild und wird NICHT an den Emulator weitergereicht.
        if key == Qt.Key_F11:
            self.toggleFullscreenRequested.emit()
            event.accept()
            return
        # Escape verlaesst ggf. das Vollbild; das Fenster ignoriert das Signal,
        # wenn kein Vollbild aktiv ist.  ESC wird zusaetzlich an den Emulator
        # gereicht (CP/A braucht die ESC-Taste), s.u.
        if key == Qt.Key_Escape and event.modifiers() == Qt.NoModifier:
            self.exitFullscreenRequested.emit()

        # Alle uebrigen Tasten an den K7637/Core weiterleiten.  Ohne diese
        # Weiterleitung erreichte bisher KEINE Host-Eingabe den Emulator.
        if self._forward_key_press(event):
            event.accept()
            return
        super().keyPressEvent(event)

    def keyReleaseEvent(self, event):
        if event.key() == Qt.Key_F11:
            event.accept()
            return
        if self._forward_key_release(event):
            event.accept()
            return
        super().keyReleaseEvent(event)

    def _forward_key_press(self, event) -> bool:
        if self.emulator is None or event.isAutoRepeat():
            return False
        mapped = self._map_key(event, press=True)
        if mapped is None:
            return False
        self.emulator.key_press(*mapped)
        return True

    def _forward_key_release(self, event) -> bool:
        if self.emulator is None or event.isAutoRepeat():
            return False
        mapped = self._map_key(event, press=False)
        if mapped is None:
            return False
        self.emulator.key_release(mapped[0])
        return True

    def focusNextPrevChild(self, weiter: bool) -> bool:
        # Tab gehoert dem emulierten Rechner. Qt fragt diese Methode VOR
        # keyPressEvent und wechselt sonst den Fokus, statt die Taste
        # weiterzureichen — der Gast saehe nie einen Tabulator.
        return False

    def focusOutEvent(self, event):
        # Ohne Fokus kommt kein Loslassen mehr an — sonst bliebe die zuletzt
        # gedrueckte Taste auf der Nachbildung fuer immer hell.
        if self.key_sink is not None:
            self.key_sink.clear_host_keys()
        super().focusOutEvent(event)

    def _map_key(self, event, press: bool):
        """Host-Taste → Kern-Tripel, wenn möglich über die Bildschirmtastatur.

        Ist eine gesetzt (:attr:`key_sink`), geht jede Host-Taste durch sie
        hindurch: sie hebt die angesprochene Taste hervor und bringt ihren
        Feststeller zur Geltung.  Auch Tasten, die der Kern nicht bekommt
        (Umschalt, Strg, Feststeller), werden ihr gezeigt — nur sichtbar
        machen lässt sich ein Modifikator sonst nicht.
        """
        sink = self.key_sink
        if sink is None:
            return qt_event_to_core_key(event)
        return (sink.host_key_press(event) if press
                else sink.host_key_release(event))

    # ── Framebuffer polling (runs outside the GL context) ────────────────────

    def _on_update(self):
        if not self.emulator or not self._powered:
            return
        # EINE Sperre fuer das ganze Bild: die Emulation laeuft in einem eigenen
        # Faden (app/emulationstakt.py), sonst wartete jeder Aufruf eine Scheibe ab.
        with kern_sperre():
            self._bild_holen()

    def _bild_holen(self):
        if not self.emulator.is_framebuffer_dirty():
            return
        # Bildgroesse kann sich mit der Maschine aendern (PC 1715: K7221/K7222).
        groesse = self.emulator.framebuffer_size()
        if groesse and groesse != (self._fb_w, self._fb_h):
            self._fb_w, self._fb_h = groesse
            self._texture_stale = True
        # Copy the framebuffer out of the core and stage it for upload.
        self._fb_bytes = self.emulator.framebuffer_bytes()
        self.emulator.clear_framebuffer_dirty_flag()
        self._pending_upload = True
        self._bild_neu()  # Neuzeichnen anstossen (die Flaeche holt sich das Bild)


class _GlFlaeche(QOpenGLWidget):
    """Die OpenGL-Zeichenflaeche (CRT-Shader) im :class:`ScreenWidget`.

    Haelt nur GL-Objekte; Bildbytes, Groesse, ``params`` und der Ein/Aus-Zustand
    liegen beim Besitzer ``_b`` (dem ScreenWidget).  Nimmt weder Fokus noch Maus
    (beides gehoert dem Besitzer).
    """

    def __init__(self, besitzer):
        super().__init__(besitzer)
        self._b = besitzer
        self.setFocusPolicy(Qt.NoFocus)
        self.setAttribute(Qt.WA_TransparentForMouseEvents)
        # GL objects (created in initializeGL, recreated on context loss).
        self._program: Optional[QOpenGLShaderProgram] = None
        self._program_einfach: Optional[QOpenGLShaderProgram] = None
        self._filter_einfach: Optional[bool] = None    # Filterzustand der Textur
        self._vbo: Optional[QOpenGLBuffer] = None
        self._vao: Optional[QOpenGLVertexArrayObject] = None
        self._texture: Optional[QOpenGLTexture] = None
        self._loc_pos = -1
        self._loc_uv = -1
        # Wall-clock for time-based effects (flicker).
        self._clock = QElapsedTimer()
        self._clock.start()

    # ── OpenGL lifecycle ─────────────────────────────────────────────────────

    def initializeGL(self):
        # QOpenGLWidget destroys and recreates its GL context whenever the widget
        # is reparented (e.g. docking/undocking or floating the screen dock).
        # Any GL object created against the OLD context must be released while
        # that context is still current, otherwise it is later freed against the
        # wrong context → segfault ("Texture has not been destroyed").  Hook the
        # context's aboutToBeDestroyed to do exactly that; initializeGL then
        # rebuilds everything on the fresh context.
        ctx = self.context()
        if ctx is not None:
            ctx.aboutToBeDestroyed.connect(self._cleanup_gl, Qt.DirectConnection)

        self._build_program()
        self._build_geometry()
        self._build_texture()

        # After a context rebuild, re-upload the last framebuffer we had.
        if self._b._fb_bytes is not None:
            self._b._pending_upload = True

    def _cleanup_gl(self):
        """Release all GL objects while the outgoing context is still current.

        Connected to ``QOpenGLContext.aboutToBeDestroyed`` (fires on reparent).
        """
        self.makeCurrent()
        try:
            if self._texture is not None:
                self._texture.destroy()
                self._texture = None
            if self._vbo is not None:
                self._vbo.destroy()
                self._vbo = None
            if self._vao is not None:
                self._vao.destroy()
                self._vao = None
            if self._program is not None:
                self._program.removeAllShaders()
                self._program = None
            if self._program_einfach is not None:
                self._program_einfach.removeAllShaders()
                self._program_einfach = None
            self._filter_einfach = None
            self._loc_pos = -1
            self._loc_uv = -1
        finally:
            self.doneCurrent()

    def _build_program(self):
        # No QObject parent: the only owner is self._program, so dropping that
        # reference in _cleanup_gl deletes the C++ object (freeing its GL
        # program) synchronously while the context is still current.
        prog = QOpenGLShaderProgram()
        if not prog.addShaderFromSourceCode(QOpenGLShader.Vertex, _VERT_SHADER):
            raise RuntimeError("CRT vertex shader failed:\n" + prog.log())
        if not prog.addShaderFromSourceCode(QOpenGLShader.Fragment, _FRAG_SHADER):
            raise RuntimeError("CRT fragment shader failed:\n" + prog.log())
        if not prog.link():
            raise RuntimeError("CRT shader link failed:\n" + prog.log())
        self._program = prog
        self._loc_pos = prog.attributeLocation("aPos")
        self._loc_uv = prog.attributeLocation("aUv")

    def _einfaches_programm(self) -> QOpenGLShaderProgram:
        """Das Programm fuer „CRT aus“ — erst beim ersten Bedarf gebaut und gebunden."""
        if self._program_einfach is None:
            prog = QOpenGLShaderProgram()
            if not prog.addShaderFromSourceCode(QOpenGLShader.Vertex, _VERT_SHADER):
                raise RuntimeError("Einfach-Vertex-Shader:\n" + prog.log())
            if not prog.addShaderFromSourceCode(QOpenGLShader.Fragment, _FRAG_SHADER_EINFACH):
                raise RuntimeError("Einfach-Fragment-Shader:\n" + prog.log())
            # Gleiche Attributplaetze wie das CRT-Programm, damit das VAO passt.
            prog.bindAttributeLocation("aPos", self._loc_pos)
            prog.bindAttributeLocation("aUv", self._loc_uv)
            if not prog.link():
                raise RuntimeError("Einfach-Shader link:\n" + prog.log())
            self._program_einfach = prog
        return self._program_einfach

    def _build_geometry(self):
        # Fullscreen quad as a triangle strip: (pos.xy, uv.xy).
        # uv (0,0) at the top-left so framebuffer row 0 renders at the top.
        import struct
        verts = [
            -1.0,  1.0, 0.0, 0.0,
            -1.0, -1.0, 0.0, 1.0,
             1.0,  1.0, 1.0, 0.0,
             1.0, -1.0, 1.0, 1.0,
        ]
        data = struct.pack("%df" % len(verts), *verts)

        self._vao = QOpenGLVertexArrayObject()   # no parent (see _build_program)
        self._vao.create()
        self._vao.bind()

        self._vbo = QOpenGLBuffer(QOpenGLBuffer.VertexBuffer)
        self._vbo.create()
        self._vbo.bind()
        self._vbo.allocate(data, len(data))

        stride = 4 * 4  # 4 floats per vertex
        self._program.bind()
        self._program.enableAttributeArray(self._loc_pos)
        self._program.setAttributeBuffer(self._loc_pos, GL_FLOAT, 0, 2, stride)
        self._program.enableAttributeArray(self._loc_uv)
        self._program.setAttributeBuffer(self._loc_uv, GL_FLOAT, 2 * 4, 2, stride)
        self._program.release()

        self._vbo.release()
        self._vao.release()

    def _build_texture(self):
        tex = QOpenGLTexture(QOpenGLTexture.Target2D)
        tex.setFormat(QOpenGLTexture.R8_UNorm)
        tex.setSize(self._b._fb_w, self._b._fb_h)
        # Full mip chain so minification (small/scaled-down window) samples an
        # averaged level instead of dropping thin glyph strokes.  Trilinear
        # (LinearMipMapLinear) min filter, plain Linear when magnifying.
        tex.setMipLevels(tex.maximumMipLevels())
        tex.setMinificationFilter(QOpenGLTexture.LinearMipMapLinear)
        tex.setMagnificationFilter(QOpenGLTexture.Linear)
        _set_anisotropy(tex)
        tex.setWrapMode(QOpenGLTexture.ClampToEdge)
        tex.allocateStorage(QOpenGLTexture.Red, QOpenGLTexture.UInt8)
        self._texture = tex

    def _upload_texture(self):
        """Upload the staged framebuffer bytes into the GL texture."""
        if self._b._fb_bytes is None or self._texture is None:
            return
        try:
            # Fast path: sub-image the raw single-channel bytes in place, then
            # rebuild the mip chain so the downscaled view stays anti-aliased.
            self._texture.setData(
                QOpenGLTexture.Red, QOpenGLTexture.UInt8, self._b._fb_bytes
            )
            if self._b.params.crt_on:
                self._texture.generateMipMaps()
        except (TypeError, RuntimeError):
            # Fallback: rebuild the texture from a grayscale QImage.  QImage is
            # top-row-first while QOpenGLTexture uploads bottom-first, so mirror.
            # The QImage ctor generates a full mip chain by default.
            img = QImage(self._b._fb_bytes, self._b._fb_w, self._b._fb_h, self._b._fb_w,
                         QImage.Format_Grayscale8).mirrored(False, True)
            if self._texture is not None:
                self._texture.destroy()
            tex = QOpenGLTexture(img)
            tex.setMinificationFilter(QOpenGLTexture.LinearMipMapLinear)
            tex.setMagnificationFilter(QOpenGLTexture.Linear)
            _set_anisotropy(tex)
            tex.setWrapMode(QOpenGLTexture.ClampToEdge)
            self._texture = tex

    def resizeGL(self, w: int, h: int):
        # Viewport is (re)computed in paintGL so we can letterbox to 4:3.
        pass

    def paintGL(self):
        f = self.context().functions()

        # Clear the whole widget (letterbox bars) to black.
        f.glClearColor(0.0, 0.0, 0.0, 1.0)
        f.glClear(GL_COLOR_BUFFER_BIT)

        # Powered off → dark tube (leave the cleared black frame).
        if not self._b._powered:
            return

        if self._program is None or self._texture is None:
            return

        if self._b._texture_stale:
            self._texture.destroy()
            self._build_texture()
            self._filter_einfach = None
            self._b._texture_stale = False
            self._b._pending_upload = self._b._fb_bytes is not None
        if self._b._pending_upload:
            self._upload_texture()
            self._b._pending_upload = False

        # Compute the largest 4:3 rectangle that fits the widget (device px).
        dpr = self.devicePixelRatioF()
        vw_full = max(1, int(round(self.width() * dpr)))
        vh_full = max(1, int(round(self.height() * dpr)))
        if vw_full / vh_full > DISPLAY_ASPECT:
            vh = vh_full
            vw = int(round(vh * DISPLAY_ASPECT))
        else:
            vw = vw_full
            vh = int(round(vw / DISPLAY_ASPECT))
        vx = (vw_full - vw) // 2
        vy = (vh_full - vh) // 2
        f.glViewport(vx, vy, vw, vh)

        einfach = not self._b.params.crt_on
        prog = self._einfaches_programm() if einfach else self._program
        # Filter nur bei Moduswechsel umstellen; ohne Mipmaps genuegt Linear.  Beim
        # Zurueckschalten auf CRT fehlt die Mipkette bis zum naechsten Bild.
        if self._filter_einfach != einfach:
            self._texture.setMinificationFilter(
                QOpenGLTexture.Linear if einfach else QOpenGLTexture.LinearMipMapLinear)
            self._filter_einfach = einfach
            if not einfach:
                self._texture.generateMipMaps()
        prog.bind()
        if einfach:
            f.glUniform3f(prog.uniformLocation("uPhosphorOn"),
                          *[float(c) for c in self._b.params.phosphor_on])
            f.glUniform3f(prog.uniformLocation("uPhosphorOff"),
                          *[float(c) for c in self._b.params.phosphor_off])
        else:
            self._set_uniforms(f)

        f.glActiveTexture(GL_TEXTURE0)
        self._texture.bind()
        f.glUniform1i(prog.uniformLocation("uScreen"), 0)

        if self._vao is not None:
            self._vao.bind()
        f.glDrawArrays(GL_TRIANGLE_STRIP, 0, 4)
        if self._vao is not None:
            self._vao.release()

        self._texture.release()
        prog.release()

    def _set_uniforms(self, f):
        """Set all shader uniforms via the typed glUniform* calls.

        NB: QOpenGLShaderProgram.setUniformValue(loc, <python float>) resolves to
        the *int* overload in PySide6 (glUniform1i) — a GL error on a float
        uniform, which then silently stays 0.  Using the explicit glUniform1f /
        glUniform2f / glUniform3f functions avoids that overload trap.
        """
        prog = self._program
        p = self._b.params

        def u(name):
            return prog.uniformLocation(name)

        f.glUniform2f(u("uResolution"), float(self._b._fb_w), float(self._b._fb_h))
        f.glUniform3f(u("uPhosphorOn"), *[float(c) for c in p.phosphor_on])
        f.glUniform3f(u("uPhosphorOff"), *[float(c) for c in p.phosphor_off])
        f.glUniform1f(u("uBrightness"), float(p.brightness))
        f.glUniform1f(u("uContrast"), float(p.contrast))
        f.glUniform1f(u("uMask"), float(p.mask_strength))
        f.glUniform1f(u("uMaskPitch"), float(p.mask_pitch))
        f.glUniform2f(u("uCurvature"), float(p.curvature[0]), float(p.curvature[1]))
        f.glUniform1f(u("uCorner"), float(p.corner_radius))
        f.glUniform1f(u("uVignette"), float(p.vignette_strength if p.vignette_on else 0.0))
        f.glUniform1f(u("uFlicker"), float(p.flicker_strength))
        f.glUniform1f(u("uTime"), float(self._clock.elapsed() / 1000.0))
        f.glUniform2f(u("uScale"), float(p.scale_x), float(p.scale_y))
        f.glUniform2f(u("uOffset"), float(p.offset_x), float(p.offset_y))


class _QtFlaeche(QWidget):
    """Schlichte Zeichenflaeche ohne OpenGL (CRT aus): ein indiziertes ``QImage`` mit 256
    Abstufungen zwischen den beiden Phosphorfarben, vom Qt-Rasterisierer auf das 4:3-Rechteck
    skaliert.  Keine Kruemmung/Vignette/Helligkeit — wie der schlichte GL-Shader.

    Gezeichnet wird nur bei neuem Bild oder Groessenaenderung; kein GL-Kontext, kein
    Fenster-grosser Framebuffer.
    """

    def __init__(self, besitzer):
        super().__init__(besitzer)
        self._b = besitzer
        self.setFocusPolicy(Qt.NoFocus)
        self.setAttribute(Qt.WA_TransparentForMouseEvents)
        self.setAttribute(Qt.WA_OpaquePaintEvent)       # wir fuellen alles selbst
        self._bild: Optional[QImage] = None
        self._farben = None

    @staticmethod
    def _tabelle(on, off, helligkeit):
        """256 Abstufungen zwischen den Phosphorfarben; beide Farben mal *helligkeit*."""
        def kan(c):
            return min(255, max(0, int(round(c * helligkeit * 255))))
        return [qRgb(*(kan(o + (n - o) * i / 255.0) for n, o in zip(on, off)))
                for i in range(256)]

    def paintEvent(self, event):
        b = self._b
        p = QPainter(self)
        p.fillRect(self.rect(), Qt.black)
        if not b._powered or b._fb_bytes is None:
            return
        w, h = b._fb_w, b._fb_h
        if len(b._fb_bytes) < w * h:                    # Groesse noch im Wechsel
            return
        if b._pending_upload or self._bild is None or self._bild.width() != w \
                or self._bild.height() != h:
            # Kein Kopieren: das QImage zeigt auf die Bytes des Besitzers (bleiben dort gehalten).
            self._bild = QImage(b._fb_bytes, w, h, w, QImage.Format_Indexed8)
            self._farben = None
            b._pending_upload = False
            b._texture_stale = False
        prm = b.params
        # Helligkeitsregler: 2,0 = Farben unveraendert, 0 = dunkel, 4 = doppelt so hell.
        faktor = max(0.0, float(prm.brightness)) / 2.0
        farben = (tuple(prm.phosphor_on), tuple(prm.phosphor_off), faktor)
        if farben != self._farben:
            self._bild.setColorTable(self._tabelle(*farben))
            self._farben = farben
        # 4:3-Roehrenflaeche, mittig im Widget.
        bw, bh = self.width(), self.height()
        if bw / max(1, bh) > DISPLAY_ASPECT:
            vh, vw = bh, int(round(bh * DISPLAY_ASPECT))
        else:
            vw, vh = bw, int(round(bw / DISPLAY_ASPECT))
        vx, vy = (bw - vw) // 2, (bh - vh) // 2
        # Ausserhalb des Rasters, aber auf der Roehre: der „unbeleuchtete“ Ton.
        p.fillRect(QRect(vx, vy, vw, vh), QColor(self._bild.color(0)))
        # Raster wie beim CRT-Shader: Groesse relativ zur Roehre, Verschiebung in Roehrenbreiten
        # je Achse (Bildzeile 0 oben, positive Y-Verschiebung = nach unten).
        rw, rh = vw * prm.scale_x, vh * prm.scale_y
        cx = vx + vw / 2.0 + prm.offset_x * vw
        cy = vy + vh / 2.0 + prm.offset_y * vh
        ziel = QRectF(cx - rw / 2.0, cy - rh / 2.0, rw, rh)
        p.setClipRect(QRect(vx, vy, vw, vh))
        p.setRenderHint(QPainter.SmoothPixmapTransform, True)
        p.drawImage(ziel, self._bild)
