# Global Object Register

All mutable module objects are file-static. The vector table is the only externally linked constant object. No heap allocation is used.


Application waveform state is held in the file-static objects `s_waveform`, `s_waveform_phase_us`, and `s_waveform_elapsed_us`. The waveform generator itself has no mutable state.
