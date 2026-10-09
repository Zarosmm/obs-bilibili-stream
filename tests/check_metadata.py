"""Check packaging identity and latest release against buildspec."""
import json
from pathlib import Path
import xml.etree.ElementTree as ET

root = Path(__file__).resolve().parents[1]
spec = json.loads((root / "buildspec.json").read_text(encoding="utf-8-sig"))
metadata = ET.parse(root / "com.obsproject.Studio.Plugin.BilibiliStream.metainfo.xml").getroot()
assert spec["platformConfig"]["macos"]["bundleId"] == metadata.findtext("id")
assert metadata.find("releases/release").get("version") == spec["version"]
print("Packaging metadata checks passed")
