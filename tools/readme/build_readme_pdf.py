#!/usr/bin/env python3
"""Build the customer Read Me, packaging/DUNGEON CRAWLER - Read Me.pdf (shipped next to
DUNGEON CRAWLER.pkg in the customer download DUNGEON-CRAWLER-1.0-macOS).

Customer-facing text only. Edit the TEXT below, then run (needs reportlab):
    python3 tools/readme/build_readme_pdf.py
and commit the PDF. The script refuses old names (old company names, script names).
"""
import pathlib
import re
import sys

from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle
from reportlab.lib.units import mm
from reportlab.lib import colors
from reportlab.platypus import Paragraph, SimpleDocTemplate, Spacer, Preformatted, KeepTogether

ROOT = pathlib.Path(__file__).resolve().parents[2]
OUT = ROOT / "packaging" / "DUNGEON CRAWLER - Read Me.pdf"

# (kind, text): h = heading, p = paragraph, b = bullet, c = path or command (monospace)
TEXT = [
    ("title", "DUNGEON CRAWLER"),
    ("sub", "by Windward Audio  ·  version 1.0.0  ·  Read Me"),
    ("h", "What it is"),
    ("p", "<b>DUNGEON CRAWLER</b> is a dark-fantasy instrument: a software instrument you play from a "
          "MIDI keyboard or the piano roll. A painted map of a dark realm is its interface, and every "
          "place on it has its own instrument, played inside that place's space and effects: a lute in "
          "the castle courtyard, church bells in the bell tower, a ghost violin in the graveyard."),
    ("p", "<b>DUNGEON CRAWLER FX</b> is the same ten locations and forty places as an audio effect: "
          "whatever runs through it (a guitar, a synth, a piano, a voice) sounds as if it were played there."),
    ("p", "Both are installed by <b>DUNGEON CRAWLER.pkg</b> as Audio Unit and VST3 plug-ins, with their "
          "sounds and factory presets."),
    ("h", "Requirements"),
    ("b", "macOS 10.15 (Catalina) or later"),
    ("b", "A Mac with Apple Silicon or an Intel processor"),
    ("b", "A music app that loads Audio Unit or VST3 plug-ins (GarageBand, Logic Pro and others)"),
    ("h", "Install"),
    ("b", "1.  Quit GarageBand, Logic Pro and any other music apps."),
    ("b", "2.  Double-click <b>DUNGEON CRAWLER.pkg</b> and follow the steps. macOS asks for your password."),
    ("b", "3.  Open your music app. Both plug-ins are listed under <b>Windward Audio</b>."),
    ("p", "Installing again (for example a later 1.0 update) replaces the plug-ins and keeps your presets."),
    ("h", "Finding it in your music app"),
    ("p", "<b>GarageBand</b>"),
    ("b", "DUNGEON CRAWLER: create a Software Instrument track, open Smart Controls (B) &gt; Plug-ins &gt; "
          "the Instrument slot &gt; AU Instruments &gt; Windward Audio &gt; DUNGEON CRAWLER."),
    ("b", "DUNGEON CRAWLER FX: on any track, Smart Controls (B) &gt; Plug-ins &gt; an empty slot &gt; "
          "Audio Units &gt; Windward Audio &gt; DUNGEON CRAWLER FX."),
    ("b", "If you see sliders instead of the painted map, switch the plug-in window's view menu from "
          "“Controls” to “Editor”."),
    ("p", "<b>Logic Pro</b>"),
    ("b", "DUNGEON CRAWLER: the Instrument slot of a Software Instrument track &gt; AU Instruments &gt; "
          "Windward Audio &gt; DUNGEON CRAWLER."),
    ("b", "DUNGEON CRAWLER FX: any Audio FX slot &gt; Audio Units &gt; Windward Audio &gt; DUNGEON CRAWLER FX."),
    ("b", "If either is missing, open Logic Pro &gt; Settings &gt; Plug-in Manager, select it under "
          "Windward Audio and click “Reset &amp; Rescan Selection”."),
    ("p", "<b>Other music apps</b>"),
    ("b", "Rescan plug-ins if your app asks, then look for Windward Audio (or DUNGEON CRAWLER / "
          "DUNGEON CRAWLER FX) in its instrument or effect list, as Audio Unit or VST3."),
    ("p", "Projects you made with an earlier version of DUNGEON CRAWLER open with the same sound and settings."),
    ("h", "Presets"),
    ("p", "The factory presets are built into the plug-ins (the Presets menu at the top of the window). "
          "Presets you save are kept in:"),
    ("c", "~/Library/Windward Audio/DUNGEON CRAWLER/Presets\n~/Library/Windward Audio/DUNGEON CRAWLER FX/Presets"),
    ("p", "Presets your music app saves for the plug-ins are kept in:"),
    ("c", "~/Library/Audio/Presets/Windward Audio/DUNGEON CRAWLER\n~/Library/Audio/Presets/Windward Audio/DUNGEON CRAWLER FX"),
    ("p", "The first time you open version 1.0, presets you saved with an earlier version are copied here "
          "automatically; the originals are left where they were. "
          "(~ is your home folder. In Finder choose Go &gt; Go to Folder and paste the path.)"),
    ("h", "Uninstall"),
    ("p", "Quit your music apps, then move these to the Trash (Finder asks for your password):"),
    ("c", "/Library/Audio/Plug-Ins/Components/DungeonCrawler.component\n"
          "/Library/Audio/Plug-Ins/Components/DungeonCrawlerFX.component\n"
          "/Library/Audio/Plug-Ins/VST3/DungeonCrawler.vst3\n"
          "/Library/Audio/Plug-Ins/VST3/DungeonCrawlerFX.vst3"),
    ("p", "To remove your saved presets as well, also move these to the Trash:"),
    ("c", "~/Library/Windward Audio/DUNGEON CRAWLER\n~/Library/Windward Audio/DUNGEON CRAWLER FX\n"
          "~/Library/Audio/Presets/Windward Audio/DUNGEON CRAWLER\n~/Library/Audio/Presets/Windward Audio/DUNGEON CRAWLER FX"),
    ("p", "Then restart your Mac (or log out and back in) so music apps forget the plug-ins."),
    ("h", "Support"),
    ("p", "Questions or problems? Write to <b>support@windwardaudio.com</b>. Please include your macOS "
          "version, your music app and what happened."),
]

FORBIDDEN = re.compile(r"veil|elsewhere|install-mac|\.sh\b|script", re.I)


def main() -> int:
    for _, text in TEXT:
        if FORBIDDEN.search(text):
            print(f"refusing a forbidden word in: {text}", file=sys.stderr)
            return 1
    ink = colors.HexColor("#1d1a17")
    accent = colors.HexColor("#7a1414")
    styles = {
        "title": ParagraphStyle("title", fontName="Helvetica-Bold", fontSize=26, leading=30, textColor=accent, spaceAfter=2),
        "sub": ParagraphStyle("sub", fontName="Helvetica", fontSize=10.5, leading=14, textColor=colors.HexColor("#5b544c"), spaceAfter=10),
        "h": ParagraphStyle("h", fontName="Helvetica-Bold", fontSize=13, leading=17, textColor=accent, spaceBefore=10, spaceAfter=4),
        "p": ParagraphStyle("p", fontName="Helvetica", fontSize=10.5, leading=14.5, textColor=ink, spaceAfter=5),
        "b": ParagraphStyle("b", fontName="Helvetica", fontSize=10.5, leading=14.5, textColor=ink, leftIndent=12, spaceAfter=3),
        "c": ParagraphStyle("c", fontName="Courier", fontSize=9.3, leading=12.5, textColor=ink, leftIndent=12, spaceBefore=1, spaceAfter=7,
                            backColor=colors.HexColor("#f2eee8"), borderPadding=(4, 6, 4, 6)),
    }
    story = []
    for kind, text in TEXT:
        if kind == "c":
            story.append(Preformatted(text, styles["c"]))
        elif kind == "b":
            story.append(Paragraph(text if re.match(r"\d\.", text) else "•  " + text, styles["b"]))
        else:
            story.append(Paragraph(text, styles[kind]))
    doc = SimpleDocTemplate(str(OUT), pagesize=A4, leftMargin=20 * mm, rightMargin=20 * mm, topMargin=18 * mm, bottomMargin=18 * mm,
                            title="DUNGEON CRAWLER - Read Me", author="Windward Audio", subject="DUNGEON CRAWLER 1.0.0",
                            creator="Windward Audio", producer="Windward Audio")
    doc.build(story)
    print(f"wrote {OUT}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
