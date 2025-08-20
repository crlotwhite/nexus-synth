#!/usr/bin/env python3
"""
Performance Regression Detection Script

Detects performance regressions by comparing current results with historical baselines.
"""

import argparse
import json
import os
import sys
from pathlib import Path
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Tuple, Any
import statistics

try:
    import pandas as pd
    import numpy as np
    from scipy import stats
except ImportError as e:
    print(f"❌ Missing required dependency: {e}")
    sys.exit(1)

class RegressionDetector:
    """Detects performance regressions in benchmark data."""
    
    def __init__(self, input_dir: Path, output_dir: Path, 
                 threshold: float = 10.0, lookback_days: int = 7):
        self.input_dir = input_dir
        self.output_dir = output_dir
        self.threshold = threshold  # Percentage threshold for regression
        self.lookback_days = lookback_days
        
        self.output_dir.mkdir(parents=True, exist_ok=True)

    def detect_regressions(self) -> Dict[str, Any]:
        """Detect performance regressions in the current results."""
        print(f"🔍 Detecting regressions with {self.threshold}% threshold")
        
        # Load all benchmark data
        all_data = self._load_all_benchmark_data()
        if not all_data:
            print("❌ No benchmark data found")
            return {'regressions': [], 'analysis_successful': False}
        
        # Group data by platform and benchmark
        grouped_data = self._group_benchmark_data(all_data)
        
        # Detect regressions
        regressions = []
        improvements = []
        
        for platform in grouped_data:
            platform_regressions, platform_improvements = self._analyze_platform_regressions(
                platform, grouped_data[platform]
            )
            regressions.extend(platform_regressions)
            improvements.extend(platform_improvements)
        
        # Generate regression report
        regression_report = {
            'analysis_timestamp': datetime.now().isoformat(),
            'threshold_percent': self.threshold,
            'lookback_days': self.lookback_days,
            'total_platforms': len(grouped_data),
            'regressions_detected': len(regressions),
            'improvements_detected': len(improvements),
            'regressions': regressions,
            'improvements': improvements,
            'analysis_successful': True
        }
        
        # Save regression report
        report_file = self.output_dir / "performance_alert.json"
        with open(report_file, 'w') as f:
            json.dump(regression_report, f, indent=2)
        
        # Generate detailed analysis
        self._generate_detailed_analysis(regression_report, grouped_data)
        
        print(f"✅ Regression analysis completed: {len(regressions)} regressions, {len(improvements)} improvements")
        
        return regression_report

    def _load_all_benchmark_data(self) -> List[Dict[str, Any]]:
        """Load all benchmark data from input directory."""
        all_data = []
        
        json_files = list(self.input_dir.rglob("*.json"))
        if not json_files:
            print(f"⚠️ No JSON files found in {self.input_dir}")
            return all_data
        
        for json_file in json_files:
            try:
                with open(json_file, 'r') as f:
                    data = json.load(f)
                    
                # Extract platform from file path
                platform = self._extract_platform_from_path(json_file)
                
                if isinstance(data, list):
                    for item in data:
                        item['_platform'] = platform
                        item['_source_file'] = str(json_file)
                        all_data.append(item)
                elif isinstance(data, dict):
                    if 'results' in data:
                        for item in data['results']:
                            item['_platform'] = platform
                            item['_source_file'] = str(json_file)
                            all_data.append(item)
                    else:
                        data['_platform'] = platform
                        data['_source_file'] = str(json_file)
                        all_data.append(data)
                        
            except (json.JSONDecodeError, KeyError) as e:
                print(f"⚠️ Failed to load {json_file}: {e}")
                continue
        
        return all_data

    def _extract_platform_from_path(self, file_path: Path) -> str:
        """Extract platform name from file path."""
        path_str = str(file_path)
        
        if "Windows" in path_str or "windows" in path_str:
            return "Windows"
        elif "Ubuntu" in path_str or "ubuntu" in path_str:
            return "Ubuntu"
        elif "macOS" in path_str or "macos" in path_str:
            return "macOS"
        else:
            return "Unknown"

    def _group_benchmark_data(self, all_data: List[Dict[str, Any]]) -> Dict[str, Dict[str, List[Dict[str, Any]]]]:
        """Group benchmark data by platform and benchmark name."""
        grouped = {}
        
        for item in all_data:
            platform = item.get('_platform', 'Unknown')
            benchmark_name = item.get('benchmark_name', 'unknown')
            
            if platform not in grouped:
                grouped[platform] = {}
            if benchmark_name not in grouped[platform]:
                grouped[platform][benchmark_name] = []
                
            grouped[platform][benchmark_name].append(item)
        
        return grouped

    def _analyze_platform_regressions(self, platform: str, 
                                    platform_data: Dict[str, List[Dict[str, Any]]]) -> Tuple[List[Dict[str, Any]], List[Dict[str, Any]]]:
        """Analyze regressions for a specific platform."""
        regressions = []
        improvements = []
        
        for benchmark_name, benchmark_data in platform_data.items():
            if len(benchmark_data) < 2:
                continue  # Need at least 2 data points for comparison
            
            # Extract timing data
            timing_data = []
            for item in benchmark_data:
                avg_time = self._safe_get_numeric(item, 'avg_execution_time_ns', 'avg_execution_time')
                if avg_time > 0:
                    timing_data.append(avg_time)
            
            if len(timing_data) < 2:
                continue
            
            # Calculate baseline (historical average) and current performance
            # For simplicity, use first half as baseline and second half as current
            mid_point = len(timing_data) // 2
            baseline_times = timing_data[:mid_point] if mid_point > 0 else timing_data[:-1]
            current_times = timing_data[mid_point:] if mid_point > 0 else timing_data[-1:]
            
            baseline_avg = statistics.mean(baseline_times)
            current_avg = statistics.mean(current_times)
            
            # Calculate percentage change
            if baseline_avg > 0:
                percent_change = ((current_avg - baseline_avg) / baseline_avg) * 100
                
                if abs(percent_change) > self.threshold:
                    regression_data = {
                        'benchmark': benchmark_name,
                        'platform': platform,
                        'baseline_time': round(baseline_avg / 1_000_000, 3),  # Convert to ms
                        'current_time': round(current_avg / 1_000_000, 3),   # Convert to ms
                        'regression_percent': round(percent_change, 2),
                        'sample_count_baseline': len(baseline_times),
                        'sample_count_current': len(current_times),
                        'severity': self._calculate_severity(percent_change)
                    }
                    
                    if percent_change > 0:  # Performance degraded
                        regressions.append(regression_data)
                    else:  # Performance improved
                        improvements.append(regression_data)
        
        return regressions, improvements

    def _safe_get_numeric(self, data: Dict[str, Any], *keys) -> float:
        """Safely extract numeric value from data."""
        for key in keys:
            if key in data:
                value = data[key]
                if isinstance(value, (int, float)):
                    return float(value)
                elif isinstance(value, str) and value.replace('.', '').isdigit():
                    return float(value)
        return 0.0

    def _calculate_severity(self, percent_change: float) -> str:
        """Calculate severity level based on percentage change."""
        abs_change = abs(percent_change)
        
        if abs_change > 50:
            return "critical"
        elif abs_change > 30:
            return "high"
        elif abs_change > 15:
            return "medium"
        else:
            return "low"

    def _generate_detailed_analysis(self, regression_report: Dict[str, Any], 
                                  grouped_data: Dict[str, Dict[str, List[Dict[str, Any]]]]) -> None:
        """Generate detailed regression analysis report."""
        
        # Create detailed HTML report
        html_file = self.output_dir / "regression_analysis_detailed.html"
        
        html_content = f"""
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Performance Regression Analysis</title>
    <style>
        body {{ 
            font-family: -apple-system, BlinkMacSystemFont, sans-serif;
            line-height: 1.6; 
            margin: 0; 
            padding: 20px; 
            background-color: #f5f5f5;
        }}
        .container {{ 
            max-width: 1200px; 
            margin: 0 auto; 
            background: white; 
            padding: 30px; 
            border-radius: 10px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }}
        h1, h2, h3 {{ color: #333; }}
        .alert {{ 
            padding: 15px; 
            margin: 15px 0; 
            border-radius: 8px; 
            border-left: 4px solid;
        }}
        .alert-critical {{ border-left-color: #dc3545; background-color: #f8d7da; }}
        .alert-high {{ border-left-color: #fd7e14; background-color: #fff3cd; }}
        .alert-medium {{ border-left-color: #ffc107; background-color: #fff3cd; }}
        .alert-low {{ border-left-color: #28a745; background-color: #d4edda; }}
        .improvement {{ border-left-color: #17a2b8; background-color: #d1ecf1; }}
        table {{ 
            width: 100%; 
            border-collapse: collapse; 
            margin: 20px 0; 
        }}
        th, td {{ 
            padding: 12px; 
            text-align: left; 
            border-bottom: 1px solid #ddd; 
        }}
        th {{ 
            background-color: #f8f9fa; 
        }}
        .metric {{ 
            display: inline-block; 
            margin: 10px; 
            padding: 15px; 
            background: #e8f4f8; 
            border-radius: 8px; 
            text-align: center;
        }}
    </style>
</head>
<body>
    <div class="container">
        <h1>🚨 Performance Regression Analysis</h1>
        <p><strong>Generated:</strong> {regression_report['analysis_timestamp']}</p>
        
        <div class="metric">
            <h3>{regression_report['regressions_detected']}</h3>
            <p>Regressions Detected</p>
        </div>
        <div class="metric">
            <h3>{regression_report['improvements_detected']}</h3>
            <p>Improvements Detected</p>
        </div>
        <div class="metric">
            <h3>{regression_report['threshold_percent']}%</h3>
            <p>Detection Threshold</p>
        </div>
        
        <h2>🐌 Performance Regressions</h2>
        """
        
        if regression_report['regressions']:
            for regression in regression_report['regressions']:
                severity_class = f"alert-{regression['severity']}"
                html_content += f"""
                <div class="alert {severity_class}">
                    <h3>{regression['benchmark']} on {regression['platform']}</h3>
                    <p><strong>Performance degraded by {regression['regression_percent']:.1f}%</strong></p>
                    <ul>
                        <li>Baseline: {regression['baseline_time']:.3f}ms</li>
                        <li>Current: {regression['current_time']:.3f}ms</li>
                        <li>Severity: {regression['severity'].title()}</li>
                    </ul>
                </div>
                """
        else:
            html_content += "<p>✅ No performance regressions detected above the threshold.</p>"
        
        html_content += "<h2>🚀 Performance Improvements</h2>"
        
        if regression_report['improvements']:
            for improvement in regression_report['improvements']:
                html_content += f"""
                <div class="alert improvement">
                    <h3>{improvement['benchmark']} on {improvement['platform']}</h3>
                    <p><strong>Performance improved by {abs(improvement['regression_percent']):.1f}%</strong></p>
                    <ul>
                        <li>Baseline: {improvement['baseline_time']:.3f}ms</li>
                        <li>Current: {improvement['current_time']:.3f}ms</li>
                    </ul>
                </div>
                """
        else:
            html_content += "<p>No significant performance improvements detected.</p>"
        
        html_content += """
        </div>
    </body>
    </html>
        """
        
        with open(html_file, 'w') as f:
            f.write(html_content)

def main():
    parser = argparse.ArgumentParser(description='Detect performance regressions')
    parser.add_argument('--input', type=Path, required=True,
                       help='Input directory with benchmark results')
    parser.add_argument('--output', type=Path, required=True,
                       help='Output directory for regression analysis')
    parser.add_argument('--threshold', type=float, default=10.0,
                       help='Regression threshold percentage (default: 10.0)')
    parser.add_argument('--lookback-days', type=int, default=7,
                       help='Days to look back for baseline (default: 7)')
    
    args = parser.parse_args()
    
    if not args.input.exists():
        print(f"❌ Input directory does not exist: {args.input}")
        return 1
    
    detector = RegressionDetector(args.input, args.output, 
                                 args.threshold, args.lookback_days)
    
    report = detector.detect_regressions()
    
    # Exit with error code if regressions detected
    if report['regressions_detected'] > 0:
        print(f"❌ {report['regressions_detected']} performance regressions detected!")
        return 1
    
    return 0

if __name__ == '__main__':
    sys.exit(main())