#pragma once

#include "utau_oto_parser.h"
#include "vcv_pattern_recognizer.h"
#include <string>
#include <vector>
#include <chrono>
#include <memory>
#include <map>
#include <optional>

namespace nexussynth {
namespace midi {

    /**
     * @brief MIDI note information structure
     */
    struct MidiNote {
        uint8_t note_number;        // MIDI note number (0-127)
        uint8_t velocity;           // Note velocity (0-127)
        uint32_t start_tick;        // Start time in MIDI ticks
        uint32_t duration_ticks;    // Duration in MIDI ticks
        std::string lyric;          // Associated lyric text
        
        // Calculated timing (filled during conversion)
        double start_time_ms;       // Start time in milliseconds
        double duration_ms;         // Duration in milliseconds
        double frequency_hz;        // Base frequency in Hz
        
        MidiNote() 
            : note_number(60), velocity(127), start_tick(0), duration_ticks(0)
            , start_time_ms(0.0), duration_ms(0.0), frequency_hz(261.63) {}
    };

    /**
     * @brief MIDI tempo change event
     */
    struct TempoEvent {
        uint32_t tick;              // MIDI tick position
        double bpm;                 // Beats per minute
        uint32_t microseconds_per_quarter; // μs per quarter note
        
        TempoEvent() : tick(0), bpm(120.0), microseconds_per_quarter(500000) {}
        TempoEvent(uint32_t t, double b) : tick(t), bpm(b) {
            microseconds_per_quarter = static_cast<uint32_t>(60000000.0 / bpm);
        }
    };

    /**
     * @brief MIDI continuous controller event
     */
    struct CCEvent {
        uint32_t tick;              // MIDI tick position
        uint8_t controller;         // CC number (0-127)
        uint8_t value;              // CC value (0-127)
        double time_ms;             // Calculated time in milliseconds
        
        CCEvent() : tick(0), controller(0), value(0), time_ms(0.0) {}
    };

    /**
     * @brief MIDI pitch bend event
     */
    struct PitchBendEvent {
        uint32_t tick;              // MIDI tick position
        int16_t value;              // Pitch bend value (-8192 to +8191)
        double time_ms;             // Calculated time in milliseconds
        double semitones;           // Calculated semitone shift
        
        PitchBendEvent() : tick(0), value(0), time_ms(0.0), semitones(0.0) {}
    };

    /**
     * @brief Tempo map for MIDI timing conversion
     */
    class TempoMap {
    public:
        TempoMap();
        explicit TempoMap(uint16_t ppqn);
        
        void addTempoEvent(const TempoEvent& event);
        void setPPQN(uint16_t ppqn) { ppqn_ = ppqn; }
        uint16_t getPPQN() const { return ppqn_; }
        
        // Convert MIDI ticks to seconds
        double ticksToSeconds(uint32_t ticks) const;
        double ticksToMilliseconds(uint32_t ticks) const;
        
        // Convert seconds to MIDI ticks
        uint32_t secondsToTicks(double seconds) const;
        uint32_t millisecondsToTicks(double milliseconds) const;
        
        // Get tempo at specific time
        double getTempoAtTick(uint32_t tick) const;
        double getTempoAtTime(double seconds) const;
        
        // Utility methods
        std::vector<TempoEvent> getTempoEvents() const { return tempo_events_; }
        void clear();
        bool empty() const { return tempo_events_.empty(); }
        
    private:
        std::vector<TempoEvent> tempo_events_;
        uint16_t ppqn_;  // Pulses Per Quarter Note
        
        void sortTempoEvents();
        std::pair<size_t, size_t> findTempoRange(uint32_t tick) const;
    };

    /**
     * @brief Phoneme timing information for MIDI integration
     */
    struct PhonemeTimingInfo {
        std::string phoneme;        // Phoneme string (e.g., "a", "ka")
        double start_time_ms;       // Start time in milliseconds
        double duration_ms;         // Phoneme duration
        double pitch_hz;            // Target pitch frequency
        
        // UTAU-specific timing parameters
        double preutterance_ms;     // Pre-utterance timing
        double overlap_ms;          // Overlap with previous phoneme
        double consonant_ms;        // Consonant portion duration
        double blank_ms;            // Blank portion duration
        
        // Quality and confidence metrics
        double timing_confidence;   // Timing accuracy confidence [0.0-1.0]
        bool is_valid;             // Whether timing is valid
        
        PhonemeTimingInfo() 
            : start_time_ms(0.0), duration_ms(0.0), pitch_hz(261.63)
            , preutterance_ms(0.0), overlap_ms(0.0), consonant_ms(0.0), blank_ms(0.0)
            , timing_confidence(1.0), is_valid(true) {}
    };

    /**
     * @brief Integrated musical note with phoneme timing
     */
    struct MusicalPhoneme {
        MidiNote midi_note;         // Original MIDI note information
        PhonemeTimingInfo timing;   // Phoneme timing information
        utau::VCVSegment vcv_info;  // VCV pattern information (if applicable)
        
        // Musical expression parameters
        double vibrato_depth;       // Vibrato depth [0.0-1.0]
        double vibrato_rate;        // Vibrato rate in Hz
        double dynamics;            // Dynamic level [0.0-1.0]
        double brightness;          // Brightness/resonance [0.0-1.0]
        
        // Pitch trajectory
        std::vector<double> pitch_curve; // F0 trajectory over time
        std::vector<double> time_points; // Time points for pitch curve
        
        MusicalPhoneme() 
            : vibrato_depth(0.0), vibrato_rate(0.0), dynamics(1.0), brightness(0.5) {}
    };

    /**
     * @brief MIDI file parser and analyzer
     */
    class MidiParser {
    public:
        struct ParseResult {
            std::vector<MidiNote> notes;
            std::vector<TempoEvent> tempo_events;
            std::vector<CCEvent> cc_events;
            std::vector<PitchBendEvent> pitch_bend_events;
            std::vector<std::string> lyrics;
            
            TempoMap tempo_map;
            uint16_t ppqn;
            double total_duration_ms;
            
            bool success;
            std::vector<std::string> errors;
            
            ParseResult() : ppqn(480), total_duration_ms(0.0), success(false) {}
        };
        
        ParseResult parseFile(const std::string& filename);
        ParseResult parseFromBuffer(const std::vector<uint8_t>& data);
        
        // Static utility methods
        static double midiNoteToFrequency(uint8_t note_number);
        static uint8_t frequencyToMidiNote(double frequency);
        static std::string midiNoteToName(uint8_t note_number);
        
    private:
        // Internal parsing methods
        bool parseHeader(const uint8_t* data, size_t& offset, ParseResult& result);
        bool parseTrack(const uint8_t* data, size_t& offset, size_t track_length, ParseResult& result);
        bool parseMetaEvent(const uint8_t* data, size_t& offset, ParseResult& result);
        bool parseMidiEvent(const uint8_t* data, size_t& offset, ParseResult& result);
        
        // Utility methods
        uint32_t readVariableLength(const uint8_t* data, size_t& offset);
        uint32_t readUint32BE(const uint8_t* data, size_t offset);
        uint16_t readUint16BE(const uint8_t* data, size_t offset);
        
        // State tracking for parsing
        uint32_t current_tick_;
        uint8_t running_status_;
    };

    /**
     * @brief MIDI to phoneme timing integrator
     */
    class MidiPhonemeIntegrator {
    public:
        struct IntegrationOptions {
            bool strict_timing_alignment;  // Strict vs. flexible timing alignment
            bool auto_detect_language;     // Auto-detect lyric language
            bool generate_vcv_patterns;    // Generate VCV patterns from lyrics
            double timing_tolerance_ms;    // Timing alignment tolerance
            double pitch_bend_range;       // Pitch bend range in semitones
            
            IntegrationOptions()
                : strict_timing_alignment(false), auto_detect_language(true)
                , generate_vcv_patterns(true), timing_tolerance_ms(50.0)
                , pitch_bend_range(2.0) {}
        };
        
        struct IntegrationResult {
            std::vector<MusicalPhoneme> musical_phonemes;
            std::vector<utau::OtoEntry> generated_oto_entries;
            double total_duration_ms;
            double timing_accuracy;        // Overall timing accuracy score
            
            bool success;
            std::vector<std::string> errors;
            std::vector<std::string> warnings;
            
            IntegrationResult() 
                : total_duration_ms(0.0), timing_accuracy(1.0), success(false) {}
        };
        
        MidiPhonemeIntegrator();
        explicit MidiPhonemeIntegrator(const IntegrationOptions& options);
        
        // Main integration methods
        IntegrationResult integrateFromMidi(const std::string& midi_file,
                                          const std::vector<utau::OtoEntry>& oto_entries);
        
        IntegrationResult integrateFromData(const MidiParser::ParseResult& midi_data,
                                          const std::vector<utau::OtoEntry>& oto_entries);
        
        // Phoneme timing calculation
        std::vector<PhonemeTimingInfo> calculatePhonemeTimings(
            const std::vector<MidiNote>& midi_notes,
            const std::vector<utau::OtoEntry>& oto_entries,
            const TempoMap& tempo_map);
        
        // Pitch trajectory generation
        std::vector<double> generatePitchCurve(const MidiNote& note,
                                             const std::vector<PitchBendEvent>& pitch_bends,
                                             const std::vector<CCEvent>& cc_events,
                                             double sample_rate = 1000.0);
        
        // Musical parameter mapping
        void mapMusicalParameters(MusicalPhoneme& phoneme,
                                const std::vector<CCEvent>& cc_events,
                                double time_ms);
        
        // Configuration
        void setOptions(const IntegrationOptions& options) { options_ = options; }
        const IntegrationOptions& getOptions() const { return options_; }
        
        // Integration with VCV recognizer
        void setVCVRecognizer(std::shared_ptr<utau::VCVPatternRecognizer> recognizer) {
            vcv_recognizer_ = recognizer;
        }
        
    private:
        IntegrationOptions options_;
        std::shared_ptr<utau::VCVPatternRecognizer> vcv_recognizer_;
        
        // Internal integration methods
        std::vector<MusicalPhoneme> alignMidiWithPhonemes(
            const std::vector<MidiNote>& midi_notes,
            const std::vector<PhonemeTimingInfo>& phoneme_timings);
        
        PhonemeTimingInfo calculateSinglePhoneTiming(
            const MidiNote& midi_note,
            const utau::OtoEntry& oto_entry,
            const TempoMap& tempo_map);
        
        // Timing alignment algorithms
        double calculateTimingAccuracy(const std::vector<MusicalPhoneme>& phonemes);
        std::vector<double> optimizePhonemeTimings(
            const std::vector<PhonemeTimingInfo>& timings);
        
        // Utility methods
        std::string extractPhonemeFromLyric(const std::string& lyric, size_t index = 0);
        bool isTimingValid(const PhonemeTimingInfo& timing);
        void validateIntegrationResult(IntegrationResult& result);
        
        // Error handling
        void addError(IntegrationResult& result, const std::string& error);
        void addWarning(IntegrationResult& result, const std::string& warning);
    };

    /**
     * @brief Real-time MIDI to phoneme converter
     */
    class RealtimeMidiConverter {
    public:
        struct RealtimeOptions {
            double buffer_size_ms;      // Audio buffer size
            double lookahead_ms;        // Lookahead time for preparation
            int max_polyphony;          // Maximum simultaneous notes
            bool enable_prediction;     // Enable note prediction
            
            RealtimeOptions()
                : buffer_size_ms(20.0), lookahead_ms(100.0)
                , max_polyphony(8), enable_prediction(true) {}
        };
        
        RealtimeMidiConverter();
        explicit RealtimeMidiConverter(const RealtimeOptions& options);
        
        // Real-time processing
        void processMidiEvent(const MidiNote& note);
        void processCCEvent(const CCEvent& cc);
        void processPitchBend(const PitchBendEvent& bend);
        
        // Buffer management
        std::vector<MusicalPhoneme> getReadyPhonemes(double current_time_ms);
        void clearBuffer();
        size_t getBufferSize() const;
        
        // Configuration
        void setOptions(const RealtimeOptions& options) { options_ = options; }
        const RealtimeOptions& getOptions() const { return options_; }
        
    private:
        RealtimeOptions options_;
        std::vector<MusicalPhoneme> phoneme_buffer_;
        double current_time_;
        
        // Internal processing
        void schedulePhoneme(const MusicalPhoneme& phoneme);
        void updateBuffer(double current_time_ms);
        bool isPhonemeReady(const MusicalPhoneme& phoneme, double current_time_ms);
    };

    /**
     * @brief Utility functions for MIDI-phoneme integration
     */
    namespace midi_utils {
        
        // Timing utilities
        double calculateNoteOnsetAccuracy(const std::vector<MusicalPhoneme>& phonemes);
        double calculatePitchAccuracy(const std::vector<MusicalPhoneme>& phonemes);
        
        // Musical parameter interpolation
        double interpolateCC(const std::vector<CCEvent>& events, double time_ms, uint8_t cc_number);
        double interpolatePitchBend(const std::vector<PitchBendEvent>& events, double time_ms);
        
        // Conversion utilities
        std::vector<utau::OtoEntry> musicalPhonemesToOtoEntries(
            const std::vector<MusicalPhoneme>& phonemes);
        std::vector<PhonemeTimingInfo> extractPhonemeTimings(
            const std::vector<MusicalPhoneme>& phonemes);
        
        // Quality metrics
        struct QualityMetrics {
            double timing_precision;   // Timing alignment quality
            double pitch_stability;    // Pitch curve smoothness
            double musical_coherence;  // Musical expression consistency
            double overall_score;      // Combined quality score
        };
        
        QualityMetrics assessIntegrationQuality(const std::vector<MusicalPhoneme>& phonemes);
        
        // Debug and analysis
        void exportTimingAnalysis(const std::vector<MusicalPhoneme>& phonemes,
                                const std::string& output_path);
        void exportPitchTrajectory(const std::vector<MusicalPhoneme>& phonemes,
                                 const std::string& output_path);
        
    } // namespace midi_utils

} // namespace midi
} // namespace nexussynth