import { initialize, type ActivationContext } from "@ableton-extensions/sdk";
import * as fs from "fs";
import * as path from "path";
import * as os from "os";

const TEMP_DIR     = path.join(os.tmpdir(), "AINewPlug");
const WATCH_FILE   = path.join(TEMP_DIR, "ai_output.wav");
const TRIGGER_FILE = path.join(TEMP_DIR, "ai_output_ready.txt");

export function activate(activation: ActivationContext) {
  const context = initialize(activation, "1.0.0");

  console.log("AINewPlug Extension active — watching for generated audio...");
  console.log("Watching for trigger at: " + TRIGGER_FILE);

  const interval = setInterval(async () => {
    if (fs.existsSync(TRIGGER_FILE) && fs.existsSync(WATCH_FILE)) {
      console.log("New AI audio detected — creating track in Ableton...");

      // Delete trigger immediately so we don't process twice
      fs.unlinkSync(TRIGGER_FILE);

      try {
        const song = context.application.song;

        // Create new audio track
        const track = await song.createAudioTrack();
        track.name = "AI Generated";

        // Place the clip at position 0
        const clip = await track.createAudioClip({
          filePath: WATCH_FILE,
          startTime: 0,
          isWarped: false,
        });

        clip.name  = "AI Output";
        clip.color = 0x9b59b6;

        console.log("AI audio clip placed successfully!");
      }
      catch (e) {
        console.error("Failed to create track or clip:", e);
      }
    }
  }, 1000);

  return () => clearInterval(interval);
}
