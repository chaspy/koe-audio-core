# Contributor instructions

- Do not commit recordings, audio files, internal information, or personal filesystem paths.
- Use only synthetic waveforms in tests.
- Run `bin/ci` (including `kagami check --format agent-brief`) before committing.
- There is no hosted CI: `bin/ci` is the gate. Paste a summary of its output into every pull request description.
- Preserve compatibility of the C API in `include/koe_audio.h`: keep existing signatures and
  structure layouts stable; increment `KOE_AUDIO_API_VERSION` for breaking changes.
