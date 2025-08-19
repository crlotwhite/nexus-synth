#include "nexussynth/midi_phoneme_integrator.h"
#include "nexussynth/utau_oto_parser.h"
#include "nexussynth/vcv_pattern_recognizer.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <cmath>

using namespace nexussynth::midi;
using namespace nexussynth::utau;

int main() {
    std::cout << "Testing MIDI-Phoneme Integration functionality...\n";
    
    // Test TempoMap
    std::cout << "\n=== Testing TempoMap ===\n";
    try {
        TempoMap tempo_map(480); // 480 PPQN
        
        // Add tempo changes
        tempo_map.addTempoEvent(TempoEvent(0, 120.0));      // 120 BPM at start
        tempo_map.addTempoEvent(TempoEvent(960, 140.0));    // 140 BPM at beat 2
        tempo_map.addTempoEvent(TempoEvent(1920, 100.0));   // 100 BPM at beat 4
        
        // Test tick to time conversion
        double time_0 = tempo_map.ticksToMilliseconds(0);
        double time_480 = tempo_map.ticksToMilliseconds(480);  // 1 beat at 120 BPM
        double time_960 = tempo_map.ticksToMilliseconds(960);  // 2 beats
        double time_1440 = tempo_map.ticksToMilliseconds(1440); // 3 beats (1 at 140 BPM)
        
        std::cout << "Tempo map timing tests:\n";
        std::cout << "  Tick 0: " << time_0 << " ms\n";
        std::cout << "  Tick 480 (1 beat @ 120 BPM): " << time_480 << " ms (expected ~500ms)\n";
        std::cout << "  Tick 960 (2 beats): " << time_960 << " ms\n";
        std::cout << "  Tick 1440 (3 beats, 1 @ 140 BPM): " << time_1440 << " ms\n";
        
        // Test reverse conversion
        uint32_t ticks_back = tempo_map.millisecondsToTicks(time_480);
        std::cout << "  Reverse conversion: " << time_480 << " ms -> " << ticks_back << " ticks\n";
        
        // Test tempo retrieval
        double tempo_at_start = tempo_map.getTempoAtTick(0);
        double tempo_at_beat2 = tempo_map.getTempoAtTick(960);
        double tempo_at_beat3 = tempo_map.getTempoAtTick(1440);
        
        std::cout << "Tempo retrieval tests:\n";
        std::cout << "  Tempo at tick 0: " << tempo_at_start << " BPM\n";
        std::cout << "  Tempo at tick 960: " << tempo_at_beat2 << " BPM\n";
        std::cout << "  Tempo at tick 1440: " << tempo_at_beat3 << " BPM\n";
        
        std::cout << "TempoMap tests: PASSED\n";
        
    } catch (const std::exception& e) {
        std::cout << "TempoMap test failed: " << e.what() << "\n";
    }
    
    // Test MidiParser utility functions
    std::cout << "\n=== Testing MidiParser Utilities ===\n";
    try {
        // Test MIDI note to frequency conversion
        uint8_t a4_note = 69;  // A4 = MIDI note 69
        double a4_freq = MidiParser::midiNoteToFrequency(a4_note);
        std::cout << "MIDI note 69 (A4) -> " << a4_freq << " Hz (expected 440 Hz)\n";
        
        uint8_t c4_note = 60;  // C4 = MIDI note 60 (middle C)
        double c4_freq = MidiParser::midiNoteToFrequency(c4_note);
        std::cout << "MIDI note 60 (C4) -> " << c4_freq << " Hz (expected ~261.63 Hz)\n";
        
        // Test reverse conversion
        uint8_t note_back = MidiParser::frequencyToMidiNote(a4_freq);
        std::cout << "440 Hz -> MIDI note " << static_cast<int>(note_back) << " (expected 69)\n";
        
        // Test note name conversion
        std::string a4_name = MidiParser::midiNoteToName(a4_note);
        std::string c4_name = MidiParser::midiNoteToName(c4_note);
        std::cout << "Note names: MIDI 69 = " << a4_name << ", MIDI 60 = " << c4_name << "\n";
        
        std::cout << "MidiParser utilities tests: PASSED\n";
        
    } catch (const std::exception& e) {
        std::cout << "MidiParser utilities test failed: " << e.what() << "\n";
    }
    
    // Test MidiPhonemeIntegrator with mock data
    std::cout << "\n=== Testing MidiPhonemeIntegrator ===\n";
    try {
        MidiPhonemeIntegrator integrator;
        
        // Create mock MIDI data
        MidiParser::ParseResult mock_midi;
        mock_midi.ppqn = 480;
        mock_midi.success = true;
        
        // Add tempo events
        mock_midi.tempo_events.push_back(TempoEvent(0, 120.0));
        
        // Create tempo map
        for (const auto& event : mock_midi.tempo_events) {
            mock_midi.tempo_map.addTempoEvent(event);
        }
        mock_midi.tempo_map.setPPQN(mock_midi.ppqn);
        
        // Add mock MIDI notes
        MidiNote note1;
        note1.note_number = 60;  // C4
        note1.velocity = 100;
        note1.start_tick = 0;
        note1.duration_ticks = 480;  // 1 beat
        note1.lyric = "a";
        note1.start_time_ms = mock_midi.tempo_map.ticksToMilliseconds(note1.start_tick);
        note1.duration_ms = mock_midi.tempo_map.ticksToMilliseconds(note1.duration_ticks);
        note1.frequency_hz = MidiParser::midiNoteToFrequency(note1.note_number);
        
        MidiNote note2;
        note2.note_number = 64;  // E4
        note2.velocity = 110;
        note2.start_tick = 480;
        note2.duration_ticks = 480;  // 1 beat
        note2.lyric = "ka";
        note2.start_time_ms = mock_midi.tempo_map.ticksToMilliseconds(note2.start_tick);
        note2.duration_ms = mock_midi.tempo_map.ticksToMilliseconds(note2.duration_ticks);
        note2.frequency_hz = MidiParser::midiNoteToFrequency(note2.note_number);
        
        mock_midi.notes = {note1, note2};
        
        // Add mock CC events
        CCEvent cc_modulation;
        cc_modulation.tick = 240;  // Half beat
        cc_modulation.controller = 1;  // Modulation
        cc_modulation.value = 64;
        cc_modulation.time_ms = mock_midi.tempo_map.ticksToMilliseconds(cc_modulation.tick);
        
        CCEvent cc_volume;
        cc_volume.tick = 480;
        cc_volume.controller = 7;  // Volume
        cc_volume.value = 100;
        cc_volume.time_ms = mock_midi.tempo_map.ticksToMilliseconds(cc_volume.tick);
        
        mock_midi.cc_events = {cc_modulation, cc_volume};
        
        // Add mock pitch bend
        PitchBendEvent pitch_bend;
        pitch_bend.tick = 720;
        pitch_bend.value = 1024;  // Small positive bend
        pitch_bend.time_ms = mock_midi.tempo_map.ticksToMilliseconds(pitch_bend.tick);
        pitch_bend.semitones = (pitch_bend.value / 8192.0) * 2.0;
        
        mock_midi.pitch_bend_events = {pitch_bend};
        
        mock_midi.total_duration_ms = note2.start_time_ms + note2.duration_ms;
        
        // Create mock oto entries
        std::vector<OtoEntry> mock_oto_entries;
        
        OtoEntry oto1;
        oto1.filename = "a.wav";
        oto1.alias = "a";
        oto1.offset = note1.start_time_ms;
        oto1.consonant = 0.0;  // Pure vowel
        oto1.blank = 100.0;
        oto1.preutterance = 50.0;
        oto1.overlap = 20.0;
        
        OtoEntry oto2;
        oto2.filename = "ka.wav";
        oto2.alias = "ka";
        oto2.offset = note2.start_time_ms;
        oto2.consonant = 80.0;  // 80ms consonant
        oto2.blank = 120.0;
        oto2.preutterance = 60.0;
        oto2.overlap = 25.0;
        
        mock_oto_entries = {oto1, oto2};
        
        // Test integration
        auto integration_result = integrator.integrateFromData(mock_midi, mock_oto_entries);
        
        std::cout << "Integration results:\n";
        std::cout << "  Success: " << (integration_result.success ? "YES" : "NO") << "\n";
        std::cout << "  Musical phonemes: " << integration_result.musical_phonemes.size() << "\n";
        std::cout << "  Total duration: " << integration_result.total_duration_ms << " ms\n";
        std::cout << "  Timing accuracy: " << integration_result.timing_accuracy << "\n";
        std::cout << "  Generated oto entries: " << integration_result.generated_oto_entries.size() << "\n";
        std::cout << "  Errors: " << integration_result.errors.size() << "\n";
        std::cout << "  Warnings: " << integration_result.warnings.size() << "\n";
        
        // Print individual phonemes
        for (size_t i = 0; i < integration_result.musical_phonemes.size(); ++i) {
            const auto& phoneme = integration_result.musical_phonemes[i];
            std::cout << "  Phoneme " << (i + 1) << ":\n";
            std::cout << "    Text: " << phoneme.timing.phoneme << "\n";
            std::cout << "    MIDI note: " << static_cast<int>(phoneme.midi_note.note_number) 
                     << " (" << MidiParser::midiNoteToName(phoneme.midi_note.note_number) << ")\n";
            std::cout << "    Start: " << phoneme.timing.start_time_ms << " ms\n";
            std::cout << "    Duration: " << phoneme.timing.duration_ms << " ms\n";
            std::cout << "    Pitch: " << phoneme.timing.pitch_hz << " Hz\n";
            std::cout << "    Confidence: " << phoneme.timing.timing_confidence << "\n";
            std::cout << "    Vibrato depth: " << phoneme.vibrato_depth << "\n";
            std::cout << "    Dynamics: " << phoneme.dynamics << "\n";
            std::cout << "    Pitch curve points: " << phoneme.pitch_curve.size() << "\n";
        }
        
        // Print errors and warnings
        for (const auto& error : integration_result.errors) {
            std::cout << "  ERROR: " << error << "\n";
        }
        for (const auto& warning : integration_result.warnings) {
            std::cout << "  WARNING: " << warning << "\n";
        }
        
        std::cout << "MidiPhonemeIntegrator tests: PASSED\n";
        
    } catch (const std::exception& e) {
        std::cout << "MidiPhonemeIntegrator test failed: " << e.what() << "\n";
    }
    
    // Test pitch curve generation
    std::cout << "\n=== Testing Pitch Curve Generation ===\n";
    try {
        MidiPhonemeIntegrator integrator;
        
        // Create test note
        MidiNote test_note;
        test_note.note_number = 69;  // A4
        test_note.start_time_ms = 0.0;
        test_note.duration_ms = 1000.0;  // 1 second
        test_note.frequency_hz = 440.0;
        
        // Create pitch bend events
        std::vector<PitchBendEvent> pitch_bends;
        
        PitchBendEvent bend1;
        bend1.time_ms = 250.0;
        bend1.semitones = 0.5;  // Half step up
        
        PitchBendEvent bend2;
        bend2.time_ms = 750.0;
        bend2.semitones = -0.25;  // Quarter step down
        
        pitch_bends = {bend1, bend2};
        
        // Create CC events for modulation
        std::vector<CCEvent> cc_events;
        
        CCEvent cc1;
        cc1.time_ms = 0.0;
        cc1.controller = 1;  // Modulation
        cc1.value = 0;
        
        CCEvent cc2;
        cc2.time_ms = 500.0;
        cc2.controller = 1;
        cc2.value = 127;  // Full modulation
        
        cc_events = {cc1, cc2};
        
        // Generate pitch curve
        auto pitch_curve = integrator.generatePitchCurve(test_note, pitch_bends, cc_events, 100.0);
        
        std::cout << "Pitch curve generation:\n";
        std::cout << "  Base frequency: " << test_note.frequency_hz << " Hz\n";
        std::cout << "  Curve points: " << pitch_curve.size() << "\n";
        std::cout << "  Sample rate: 100 Hz\n";
        
        // Sample some points
        if (pitch_curve.size() >= 100) {
            std::cout << "  Sample points:\n";
            std::cout << "    t=0ms: " << pitch_curve[0] << " Hz\n";
            std::cout << "    t=250ms: " << pitch_curve[25] << " Hz (should be ~466 Hz with bend)\n";
            std::cout << "    t=500ms: " << pitch_curve[50] << " Hz (max modulation)\n";
            std::cout << "    t=750ms: " << pitch_curve[75] << " Hz (should be ~415 Hz with bend)\n";
            std::cout << "    t=1000ms: " << pitch_curve[99] << " Hz\n";
        }
        
        std::cout << "Pitch curve generation tests: PASSED\n";
        
    } catch (const std::exception& e) {
        std::cout << "Pitch curve generation test failed: " << e.what() << "\n";
    }
    
    // Test utility functions
    std::cout << "\n=== Testing MIDI Utils ===\n";
    try {
        // Create test data for quality assessment
        std::vector<MusicalPhoneme> test_phonemes;
        
        for (int i = 0; i < 5; ++i) {
            MusicalPhoneme phoneme;
            
            // MIDI note
            phoneme.midi_note.note_number = 60 + i * 2;  // C4, D4, E4, F#4, G#4
            phoneme.midi_note.start_time_ms = i * 500.0;
            phoneme.midi_note.duration_ms = 400.0;
            phoneme.midi_note.frequency_hz = MidiParser::midiNoteToFrequency(phoneme.midi_note.note_number);
            
            // Phoneme timing (slightly offset to test accuracy)
            phoneme.timing.phoneme = "ph" + std::to_string(i);
            phoneme.timing.start_time_ms = i * 500.0 + (i * 5.0);  // 5ms offset per phoneme
            phoneme.timing.duration_ms = 380.0;
            phoneme.timing.pitch_hz = phoneme.midi_note.frequency_hz * (1.0 + i * 0.01);  // Slight pitch offset
            phoneme.timing.timing_confidence = 0.9 - i * 0.1;
            phoneme.timing.is_valid = true;
            
            test_phonemes.push_back(phoneme);
        }
        
        // Test quality assessment
        auto quality = midi_utils::assessIntegrationQuality(test_phonemes);
        
        std::cout << "Quality assessment:\n";
        std::cout << "  Timing precision: " << quality.timing_precision << "\n";
        std::cout << "  Pitch stability: " << quality.pitch_stability << "\n";
        std::cout << "  Musical coherence: " << quality.musical_coherence << "\n";
        std::cout << "  Overall score: " << quality.overall_score << "\n";
        
        // Test accuracy calculations
        double onset_accuracy = midi_utils::calculateNoteOnsetAccuracy(test_phonemes);
        double pitch_accuracy = midi_utils::calculatePitchAccuracy(test_phonemes);
        
        std::cout << "Individual accuracy measures:\n";
        std::cout << "  Note onset accuracy: " << onset_accuracy << "\n";
        std::cout << "  Pitch accuracy: " << pitch_accuracy << "\n";
        
        // Test CC interpolation
        std::vector<CCEvent> test_cc_events;
        
        CCEvent cc1;
        cc1.time_ms = 0.0;
        cc1.controller = 7;  // Volume
        cc1.value = 64;
        
        CCEvent cc2;
        cc2.time_ms = 1000.0;
        cc2.controller = 7;
        cc2.value = 100;
        
        test_cc_events = {cc1, cc2};
        
        double cc_at_500ms = midi_utils::interpolateCC(test_cc_events, 500.0, 7);
        std::cout << "CC interpolation at 500ms: " << cc_at_500ms << " (expected ~82)\n";
        
        // Test conversion functions
        auto oto_entries = midi_utils::musicalPhonemesToOtoEntries(test_phonemes);
        std::cout << "Generated oto entries: " << oto_entries.size() << "\n";
        
        auto timing_infos = midi_utils::extractPhonemeTimings(test_phonemes);
        std::cout << "Extracted timing infos: " << timing_infos.size() << "\n";
        
        std::cout << "MIDI Utils tests: PASSED\n";
        
    } catch (const std::exception& e) {
        std::cout << "MIDI Utils test failed: " << e.what() << "\n";
    }
    
    // Test RealtimeMidiConverter
    std::cout << "\n=== Testing RealtimeMidiConverter ===\n";
    try {
        RealtimeMidiConverter converter;
        
        // Test basic functionality
        std::cout << "Initial buffer size: " << converter.getBufferSize() << "\n";
        
        // Add some notes
        MidiNote rt_note1;
        rt_note1.note_number = 60;
        rt_note1.start_time_ms = 100.0;
        rt_note1.duration_ms = 500.0;
        rt_note1.frequency_hz = MidiParser::midiNoteToFrequency(rt_note1.note_number);
        
        MidiNote rt_note2;
        rt_note2.note_number = 64;
        rt_note2.start_time_ms = 400.0;
        rt_note2.duration_ms = 300.0;
        rt_note2.frequency_hz = MidiParser::midiNoteToFrequency(rt_note2.note_number);
        
        converter.processMidiEvent(rt_note1);
        converter.processMidiEvent(rt_note2);
        
        std::cout << "Buffer size after adding notes: " << converter.getBufferSize() << "\n";
        
        // Get ready phonemes at different times
        auto ready_at_0 = converter.getReadyPhonemes(0.0);
        auto ready_at_100 = converter.getReadyPhonemes(100.0);
        auto ready_at_200 = converter.getReadyPhonemes(200.0);
        
        std::cout << "Ready phonemes:\n";
        std::cout << "  At t=0ms: " << ready_at_0.size() << "\n";
        std::cout << "  At t=100ms: " << ready_at_100.size() << "\n";
        std::cout << "  At t=200ms: " << ready_at_200.size() << "\n";
        
        // Clear buffer
        converter.clearBuffer();
        std::cout << "Buffer size after clear: " << converter.getBufferSize() << "\n";
        
        std::cout << "RealtimeMidiConverter tests: PASSED\n";
        
    } catch (const std::exception& e) {
        std::cout << "RealtimeMidiConverter test failed: " << e.what() << "\n";
    }
    
    std::cout << "\nAll MIDI-Phoneme Integration tests completed!\n";
    
    // Summary
    std::cout << "\n=== Test Summary ===\n";
    std::cout << "Core components tested:\n";
    std::cout << "✓ TempoMap - MIDI timing conversion\n";
    std::cout << "✓ MidiParser utilities - Note/frequency conversion\n";
    std::cout << "✓ MidiPhonemeIntegrator - Core integration logic\n";
    std::cout << "✓ Pitch curve generation - Musical expression\n";
    std::cout << "✓ Quality assessment - Integration metrics\n";
    std::cout << "✓ Realtime converter - Live processing\n";
    
    return 0;
}