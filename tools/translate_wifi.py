# PlatformIO pre-build: translate only known native button labels.
Import("env")
from pathlib import Path
import json

root = Path(env.subst("$PROJECT_LIBDEPS_DIR")) / env.subst("$PIOENV") / "WiFiManager"
header = root / "wm_strings_en.h"
version = json.loads((root / "library.json").read_text(encoding="utf-8"))["version"]
if version != "2.0.17":
    raise RuntimeError("Wordclock translation requires WiFiManager 2.0.17")
translations = (
    (b">Configure WiFi</button>", b">Connecter mon horloge</button>"),
    (b">Save</button>", b">Se connecter</button>"),
    (b">Refresh</button>", b">Actualiser</button>"),
)
original = header.read_bytes()
updated = original
for old, new in translations:
    if updated.count(old) == 1 and updated.count(new) == 0:
        updated = updated.replace(old, new, 1)
    elif updated.count(old) == 0 and updated.count(new) == 1:
        continue
    else:
        raise RuntimeError("Unexpected WiFiManager button markup: " + repr(old))
if updated != original:
    header.write_bytes(updated)
print("Wordclock: native WiFiManager button translations verified")
