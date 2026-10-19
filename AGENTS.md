# Contributor instructions

- Do not commit recordings, audio files, internal information, or personal filesystem paths.
- Use only synthetic waveforms in tests.
- Run `bin/ci` (including `kagami check --format agent-brief`) before committing.
- Preserve compatibility of the C API in `include/koe_audio.h`.
