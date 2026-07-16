"""Local-webcam entrypoint for the same observation-only PC pipeline."""
import os

os.environ.setdefault("PC_SOURCE", "webcam")

from cam_bridge import main

if __name__ == "__main__":
    main()
