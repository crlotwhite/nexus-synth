#!/usr/bin/env python3
"""
Test Performance Regression Detection System

This script generates synthetic performance data to test and validate
the regression detection algorithms and ensure they work correctly.
"""

import json
import random
import tempfile
from pathlib import Path
from datetime import datetime, timedelta
from typing import List, Dict, Any
import subprocess
import sys


def generate_synthetic_benchmark_data(benchmark_name: str, platform: str, 
                                     base_time_ms: float, variability: float = 0.1,
                                     has_regression: bool = False, 
                                     regression_severity: float = 0.0) -> Dict[str, Any]:
    """Generate synthetic benchmark data with optional regression."""
    
    # Base execution time in nanoseconds
    base_time_ns = base_time_ms * 1_000_000
    
    # Apply regression if specified
    if has_regression:
        base_time_ns *= (1.0 + regression_severity)
    
    # Add random variability
    variance = base_time_ns * variability
    execution_time_ns = max(1000, base_time_ns + random.gauss(0, variance))
    
    # Generate other synthetic metrics
    memory_usage = random.randint(10_000_000, 100_000_000)  # 10-100 MB
    quality_score = max(0.0, min(1.0, random.gauss(0.85, 0.1)))
    
    return {
        "benchmark_name": benchmark_name,
        "avg_execution_time_ns": int(execution_time_ns),
        "min_execution_time_ns": int(execution_time_ns * 0.9),
        "max_execution_time_ns": int(execution_time_ns * 1.1),
        "avg_memory_usage": memory_usage,
        "peak_allocation": int(memory_usage * 1.2),
        "formant_preservation_score": quality_score,
        "benchmark_successful": True,
        "concurrent_threads": 1,
        "environment_info": {
            "cpu_count": 8,
            "memory_total": 16 * 1024 * 1024 * 1024,  # 16 GB
            "platform_version": platform
        }
    }


def create_test_scenarios() -> List[Dict[str, Any]]:
    """Create various test scenarios for regression detection."""
    
    scenarios = [
        {
            "name": "No Regression - Normal Variation",
            "description": "Baseline performance with normal statistical variation",
            "benchmarks": [
                {"name": "WORLD_Analysis", "base_time": 45.2, "regression": 0.0},
                {"name": "HMM_Training", "base_time": 128.7, "regression": 0.0},
                {"name": "MLPG_Generation", "base_time": 32.1, "regression": 0.0},
                {"name": "Synthesis_PbP", "base_time": 15.8, "regression": 0.0}
            ]
        },
        {
            "name": "Minor Regression - 8% Degradation", 
            "description": "Minor performance regression (should trigger minor alert)",
            "benchmarks": [
                {"name": "WORLD_Analysis", "base_time": 45.2, "regression": 0.08},
                {"name": "HMM_Training", "base_time": 128.7, "regression": 0.0},
                {"name": "MLPG_Generation", "base_time": 32.1, "regression": 0.0},
                {"name": "Synthesis_PbP", "base_time": 15.8, "regression": 0.0}
            ]
        },
        {
            "name": "Moderate Regression - 18% Degradation",
            "description": "Moderate performance regression (should trigger moderate alert)",
            "benchmarks": [
                {"name": "WORLD_Analysis", "base_time": 45.2, "regression": 0.0},
                {"name": "HMM_Training", "base_time": 128.7, "regression": 0.18},
                {"name": "MLPG_Generation", "base_time": 32.1, "regression": 0.0},
                {"name": "Synthesis_PbP", "base_time": 15.8, "regression": 0.0}
            ]
        },
        {
            "name": "Major Regression - 35% Degradation",
            "description": "Major performance regression (should trigger major alert)",
            "benchmarks": [
                {"name": "WORLD_Analysis", "base_time": 45.2, "regression": 0.0},
                {"name": "HMM_Training", "base_time": 128.7, "regression": 0.0},
                {"name": "MLPG_Generation", "base_time": 32.1, "regression": 0.35},
                {"name": "Synthesis_PbP", "base_time": 15.8, "regression": 0.0}
            ]
        },
        {
            "name": "Critical Regression - 65% Degradation",
            "description": "Critical performance regression (should trigger critical alert)",
            "benchmarks": [
                {"name": "WORLD_Analysis", "base_time": 45.2, "regression": 0.0},
                {"name": "HMM_Training", "base_time": 128.7, "regression": 0.0},
                {"name": "MLPG_Generation", "base_time": 32.1, "regression": 0.0},
                {"name": "Synthesis_PbP", "base_time": 15.8, "regression": 0.65}
            ]
        },
        {
            "name": "Multiple Regressions",
            "description": "Multiple benchmarks showing various levels of regression",
            "benchmarks": [
                {"name": "WORLD_Analysis", "base_time": 45.2, "regression": 0.12},
                {"name": "HMM_Training", "base_time": 128.7, "regression": 0.28},
                {"name": "MLPG_Generation", "base_time": 32.1, "regression": 0.08},
                {"name": "Synthesis_PbP", "base_time": 15.8, "regression": 0.45}
            ]
        },
        {
            "name": "Performance Improvement",
            "description": "Performance improvements (should not trigger alerts)",
            "benchmarks": [
                {"name": "WORLD_Analysis", "base_time": 45.2, "regression": -0.15},
                {"name": "HMM_Training", "base_time": 128.7, "regression": -0.08},
                {"name": "MLPG_Generation", "base_time": 32.1, "regression": -0.22},
                {"name": "Synthesis_PbP", "base_time": 15.8, "regression": -0.05}
            ]
        }
    ]
    
    return scenarios


def generate_baseline_data(temp_dir: Path, num_commits: int = 20):
    """Generate baseline performance data for multiple commits."""
    
    print(f"📊 Generating baseline data ({num_commits} commits)...")
    
    platforms = ["Ubuntu GCC", "Windows MSVC", "macOS Clang"]
    benchmarks = ["WORLD_Analysis", "HMM_Training", "MLPG_Generation", "Synthesis_PbP"]
    base_times = [45.2, 128.7, 32.1, 15.8]  # Base execution times in ms
    
    for i in range(num_commits):
        commit_time = datetime.now() - timedelta(days=30-i)
        
        for platform in platforms:
            platform_data = []
            
            for benchmark, base_time in zip(benchmarks, base_times):
                data = generate_synthetic_benchmark_data(
                    benchmark, platform, base_time, 
                    variability=0.15,  # 15% normal variation
                    has_regression=False
                )
                platform_data.append(data)
            
            # Save platform data
            platform_dir = temp_dir / "baseline" / platform.replace(" ", "_")
            platform_dir.mkdir(parents=True, exist_ok=True)
            
            filename = f"benchmark_results_{commit_time.strftime('%Y%m%d_%H%M%S')}.json"
            with open(platform_dir / filename, 'w') as f:
                json.dump(platform_data, f, indent=2)


def run_test_scenario(scenario: Dict[str, Any], temp_dir: Path, 
                     regression_script: Path) -> Dict[str, Any]:
    """Run a single test scenario and return results."""
    
    print(f"\n🧪 Running test scenario: {scenario['name']}")
    print(f"📝 Description: {scenario['description']}")
    
    # Generate current benchmark data for this scenario
    platforms = ["Ubuntu GCC", "Windows MSVC", "macOS Clang"]
    current_dir = temp_dir / "current"
    current_dir.mkdir(exist_ok=True)
    
    for platform in platforms:
        platform_data = []
        
        for benchmark_config in scenario['benchmarks']:
            data = generate_synthetic_benchmark_data(
                benchmark_config['name'], 
                platform,
                benchmark_config['base_time'],
                variability=0.1,
                has_regression=benchmark_config['regression'] != 0.0,
                regression_severity=benchmark_config['regression']
            )
            platform_data.append(data)
        
        # Save current data
        platform_dir = current_dir / platform.replace(" ", "_")
        platform_dir.mkdir(parents=True, exist_ok=True)
        
        with open(platform_dir / "benchmark_results_current.json", 'w') as f:
            json.dump(platform_data, f, indent=2)
    
    # Run regression detection
    db_path = temp_dir / "test_performance.db"
    output_dir = temp_dir / "regression_analysis"
    
    try:
        # First populate database with baseline data
        baseline_cmd = [
            sys.executable, str(regression_script),
            "--input", str(temp_dir / "baseline"),
            "--database", str(db_path),
            "--output", str(output_dir),
            "--commit-hash", "baseline_commit_123456"
        ]
        
        baseline_result = subprocess.run(
            baseline_cmd, 
            capture_output=True, 
            text=True,
            cwd=temp_dir.parent.parent
        )
        
        if baseline_result.returncode != 0:
            return {
                "scenario": scenario['name'],
                "success": False,
                "error": f"Baseline population failed: {baseline_result.stderr}",
                "alerts": []
            }
        
        # Then analyze current data
        analysis_cmd = [
            sys.executable, str(regression_script),
            "--input", str(current_dir),
            "--database", str(db_path),
            "--output", str(output_dir),
            "--commit-hash", "current_test_789abc"
        ]
        
        analysis_result = subprocess.run(
            analysis_cmd,
            capture_output=True,
            text=True,
            cwd=temp_dir.parent.parent
        )
        
        if analysis_result.returncode != 0:
            return {
                "scenario": scenario['name'],
                "success": False,
                "error": f"Analysis failed: {analysis_result.stderr}",
                "alerts": []
            }
        
        # Parse results
        regression_summary_path = output_dir / "regression_summary.md"
        alerts_found = []
        
        if regression_summary_path.exists():
            with open(regression_summary_path, 'r') as f:
                content = f.read()
                if "No performance regressions detected" not in content:
                    # Parse alert information from the markdown
                    lines = content.split('\n')
                    for line in lines:
                        if 'regressions' in line.lower() and '**' in line:
                            alerts_found.append(line.strip())
        
        return {
            "scenario": scenario['name'],
            "success": True,
            "error": None,
            "alerts": alerts_found,
            "stdout": analysis_result.stdout,
            "expected_regressions": [b for b in scenario['benchmarks'] if b['regression'] > 0.05]
        }
        
    except Exception as e:
        return {
            "scenario": scenario['name'],
            "success": False,
            "error": str(e),
            "alerts": []
        }


def validate_test_results(results: List[Dict[str, Any]]) -> Dict[str, Any]:
    """Validate test results and generate summary."""
    
    print("\n📋 Test Results Summary")
    print("=" * 50)
    
    total_tests = len(results)
    passed_tests = 0
    failed_tests = 0
    validation_issues = []
    
    for result in results:
        scenario_name = result['scenario']
        success = result['success']
        
        if not success:
            print(f"❌ {scenario_name}: FAILED")
            print(f"   Error: {result['error']}")
            failed_tests += 1
            continue
        
        expected_regressions = result.get('expected_regressions', [])
        alerts_found = result.get('alerts', [])
        
        # Validate expectations
        should_have_alerts = len(expected_regressions) > 0
        has_alerts = len(alerts_found) > 0
        
        if should_have_alerts == has_alerts:
            print(f"✅ {scenario_name}: PASSED")
            if has_alerts:
                print(f"   Expected regressions: {len(expected_regressions)}")
                print(f"   Alerts generated: {len(alerts_found)}")
            passed_tests += 1
        else:
            print(f"❌ {scenario_name}: VALIDATION FAILED")
            print(f"   Expected alerts: {should_have_alerts}, Found alerts: {has_alerts}")
            validation_issues.append({
                'scenario': scenario_name,
                'expected': should_have_alerts,
                'found': has_alerts,
                'expected_count': len(expected_regressions),
                'found_count': len(alerts_found)
            })
            failed_tests += 1
    
    print("\n" + "=" * 50)
    print(f"📊 Total Tests: {total_tests}")
    print(f"✅ Passed: {passed_tests}")
    print(f"❌ Failed: {failed_tests}")
    print(f"🎯 Success Rate: {(passed_tests/total_tests)*100:.1f}%")
    
    return {
        "total_tests": total_tests,
        "passed_tests": passed_tests,
        "failed_tests": failed_tests,
        "success_rate": (passed_tests/total_tests)*100,
        "validation_issues": validation_issues
    }


def main():
    print("🚀 Starting Performance Regression Detection System Test")
    print("=" * 60)
    
    # Find the regression detection script
    script_dir = Path(__file__).parent
    regression_script = script_dir / "performance_regression_detector.py"
    
    if not regression_script.exists():
        print(f"❌ Regression detection script not found: {regression_script}")
        return 1
    
    # Create temporary directory for test data
    with tempfile.TemporaryDirectory() as temp_dir_str:
        temp_dir = Path(temp_dir_str)
        
        # Generate baseline data
        generate_baseline_data(temp_dir)
        
        # Get test scenarios
        scenarios = create_test_scenarios()
        print(f"🧪 Running {len(scenarios)} test scenarios...")
        
        # Run all test scenarios
        results = []
        for scenario in scenarios:
            result = run_test_scenario(scenario, temp_dir, regression_script)
            results.append(result)
        
        # Validate and summarize results
        summary = validate_test_results(results)
        
        # Generate detailed test report
        report_path = temp_dir.parent / "regression_test_report.json"
        test_report = {
            "test_timestamp": datetime.now().isoformat(),
            "summary": summary,
            "detailed_results": results,
            "scenarios_tested": scenarios
        }
        
        try:
            with open(report_path, 'w') as f:
                json.dump(test_report, f, indent=2)
            print(f"\n📄 Detailed test report saved: {report_path}")
        except Exception as e:
            print(f"⚠️ Could not save test report: {e}")
        
        # Return success/failure based on results
        if summary['failed_tests'] == 0:
            print("\n🎉 All tests passed! Regression detection system is working correctly.")
            return 0
        else:
            print(f"\n❌ {summary['failed_tests']} tests failed. Please review the results.")
            return 1


if __name__ == '__main__':
    sys.exit(main())