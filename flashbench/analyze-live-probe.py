"""Summarize locally captured probe images/state; never uploads captures."""
import json
import pathlib
import sys

import numpy as np
from PIL import Image

root = pathlib.Path(sys.argv[1])
filtered = np.asarray(Image.open(root / "filtered.bmp").convert("RGB"), dtype=np.int16)
baseline = np.asarray(Image.open(root / "tone-only.bmp").convert("RGB"), dtype=np.int16)
delta = np.max(np.abs(filtered - baseline), axis=2)
result = {
    "pixels_changed_over_2_codes": int((delta > 2).sum()),
    "fraction_changed_over_2_codes": float((delta > 2).mean()),
    "maximum_channel_change_codes": int(delta.max()),
}
if (root / "state-0.bin").exists():
    states = []
    for plane in range(3):
        with (root / f"state-{plane}.bin").open("rb") as f:
            width, height = np.fromfile(f, dtype="<u4", count=2)
            states.append(np.fromfile(f, dtype="<f4").reshape(height, width, 4))
    phase, envelope, track = states
    active = envelope[:, :, 2] > 0
    confirmed = phase[:, :, 3] >= 1
    result.update(
        active_cells=int(active.sum()),
        active_confirmed_cells=int((active & confirmed).sum()),
        active_unconfirmed_cells=int((active & ~confirmed).sum()),
        active_moving_cells=int((active & (np.linalg.norm(track[:, :, :2], axis=2) > .5)).sum()),
    )
    changed = delta[:int(height)*2, :int(width)*2].reshape(height, 2, width, 2).max(axis=(1, 3)) > 2
    result.update(
        changed_cells=int(changed.sum()),
        changed_unconfirmed_cells=int((changed & ~confirmed).sum()),
        changed_confirmed_cells=int((changed & confirmed).sum()),
    )
print(json.dumps(result, indent=2))
