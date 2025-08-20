#!/usr/bin/env python3
"""
Stability Analysis Script for Long-Running Tests

Analyzes long-duration benchmark results to detect stability issues,
memory leaks, and performance degradation over time.
"""

import argparse
import json
import os
import sys
from pathlib import Path
from datetime import datetime
from typing import Dict, List, Optional, Tuple, Any
import statistics

try:
    import pandas as pd
    import matplotlib.pyplot as plt
    import numpy as np
    from scipy import stats
    import seaborn as sns
except ImportError as e:
    print(f"❌ Missing required dependency: {e}")
    sys.exit(1)

class StabilityAnalyzer:
    """Analyzes stability and long-term performance characteristics."""
    
    def __init__(self, input_dir: Path, output_dir: Path):
        self.input_dir = input_dir
        self.output_dir = output_dir
        
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
        # Configure matplotlib for headless environments
        plt.switch_backend('Agg')
        sns.set_style("whitegrid")
        plt.rcParams['figure.figsize'] = (14, 10)
        plt.rcParams['font.size'] = 10

    def analyze_stability(self) -> Dict[str, Any]:
        """Perform comprehensive stability analysis."""
        print(f"🔍 Starting stability analysis on {self.input_dir}")
        
        # Load stability test data
        stability_data = self._load_stability_data()
        if not stability_data:
            print("❌ No stability data found")
            return {'analysis_successful': False}
        
        print(f"📊 Loaded {len(stability_data)} stability data points")
        
        # Perform various stability analyses
        stability_report = {
            'analysis_timestamp': datetime.now().isoformat(),
            'total_data_points': len(stability_data),
            'analysis_successful': True,
            'stability_metrics': {},
            'issues_detected': [],
            'recommendations': []
        }
        
        # Analyze different stability aspects
        self._analyze_performance_stability(stability_data, stability_report)
        self._analyze_memory_stability(stability_data, stability_report)
        self._analyze_threading_stability(stability_data, stability_report)
        self._detect_stability_issues(stability_data, stability_report)
        
        # Generate visualizations
        self._generate_stability_visualizations(stability_data)
        
        # Save stability report
        report_file = self.output_dir / "stability_report.json"
        with open(report_file, 'w') as f:
            json.dump(stability_report, f, indent=2)
        
        # Generate HTML report
        self._generate_html_stability_report(stability_report, stability_data)
        
        print(f"✅ Stability analysis completed: {len(stability_report['issues_detected'])} issues detected")
        
        return stability_report

    def _load_stability_data(self) -> List[Dict[str, Any]]:
        """Load stability test data from JSON files."""
        all_data = []
        
        json_files = list(self.input_dir.rglob("*.json"))
        for json_file in json_files:
            try:
                with open(json_file, 'r') as f:
                    data = json.load(f)
                    
                if isinstance(data, list):
                    all_data.extend(data)
                elif isinstance(data, dict) and 'results' in data:
                    all_data.extend(data['results'])
                else:
                    all_data.append(data)
                    
            except (json.JSONDecodeError, KeyError):
                continue
        
        return all_data

    def _analyze_performance_stability(self, data: List[Dict[str, Any]], report: Dict[str, Any]) -> None:
        """Analyze performance stability over time."""
        print("📈 Analyzing performance stability...")
        
        # Group by benchmark name
        benchmarks = {}
        for item in data:
            benchmark_name = item.get('benchmark_name', 'unknown')
            if benchmark_name not in benchmarks:
                benchmarks[benchmark_name] = []
            
            timing = self._safe_get_numeric(item, 'avg_execution_time_ns', 'avg_execution_time')
            if timing > 0:
                benchmarks[benchmark_name].append(timing)
        
        performance_metrics = {}
        for benchmark, timings in benchmarks.items():
            if len(timings) < 10:  # Need sufficient data points
                continue
                
            # Calculate stability metrics
            mean_time = statistics.mean(timings)
            std_time = statistics.stdev(timings)
            cv = std_time / mean_time if mean_time > 0 else 0
            
            # Trend analysis (simple linear regression)
            x = list(range(len(timings)))
            slope, intercept, r_value, p_value, std_err = stats.linregress(x, timings)
            
            performance_metrics[benchmark] = {
                'mean_time_ns': mean_time,
                'std_deviation_ns': std_time,
                'coefficient_of_variation': cv,
                'trend_slope_ns_per_iteration': slope,
                'trend_r_squared': r_value ** 2,
                'trend_p_value': p_value,
                'stability_score': max(0, 1.0 - cv),  # Lower CV = higher stability
                'sample_count': len(timings)
            }
            
            # Detect performance degradation trend
            if p_value < 0.05 and slope > mean_time * 0.001:  # Statistically significant upward trend
                report['issues_detected'].append({
                    'type': 'performance_degradation',
                    'benchmark': benchmark,
                    'description': f'Statistically significant performance degradation detected',
                    'trend_slope': slope,
                    'p_value': p_value,
                    'severity': 'medium' if slope > mean_time * 0.01 else 'low'
                })
            
            # Detect high variability
            if cv > 0.2:  # 20% coefficient of variation threshold
                report['issues_detected'].append({
                    'type': 'high_performance_variability',
                    'benchmark': benchmark,
                    'description': f'High performance variability detected (CV: {cv:.2%})',
                    'coefficient_of_variation': cv,
                    'severity': 'high' if cv > 0.5 else 'medium'
                })
        
        report['stability_metrics']['performance'] = performance_metrics

    def _analyze_memory_stability(self, data: List[Dict[str, Any]], report: Dict[str, Any]) -> None:
        """Analyze memory usage stability."""
        print("🧠 Analyzing memory stability...")
        
        memory_data = []
        for item in data:
            memory_usage = item.get('avg_memory_usage', 0)
            peak_allocation = item.get('peak_allocation', 0)
            benchmark_name = item.get('benchmark_name', 'unknown')
            
            if memory_usage > 0 or peak_allocation > 0:
                memory_data.append({
                    'benchmark': benchmark_name,
                    'avg_memory': memory_usage,
                    'peak_memory': peak_allocation
                })
        
        if not memory_data:
            return
        
        df = pd.DataFrame(memory_data)
        memory_metrics = {}
        
        for benchmark in df['benchmark'].unique():
            benchmark_data = df[df['benchmark'] == benchmark]
            
            if len(benchmark_data) < 5:
                continue
            
            avg_memory_values = benchmark_data['avg_memory'].values
            peak_memory_values = benchmark_data['peak_memory'].values
            
            # Memory growth trend analysis
            x = list(range(len(avg_memory_values)))
            
            if len(avg_memory_values) > 1:
                avg_slope, _, avg_r, avg_p, _ = stats.linregress(x, avg_memory_values)
            else:
                avg_slope = avg_r = avg_p = 0
                
            if len(peak_memory_values) > 1:
                peak_slope, _, peak_r, peak_p, _ = stats.linregress(x, peak_memory_values)
            else:
                peak_slope = peak_r = peak_p = 0
            
            memory_metrics[benchmark] = {
                'avg_memory_mean': float(avg_memory_values.mean()),
                'avg_memory_std': float(avg_memory_values.std()),
                'peak_memory_mean': float(peak_memory_values.mean()),
                'peak_memory_std': float(peak_memory_values.std()),
                'avg_memory_trend_slope': avg_slope,
                'peak_memory_trend_slope': peak_slope,
                'avg_memory_trend_p_value': avg_p,
                'peak_memory_trend_p_value': peak_p,
                'sample_count': len(benchmark_data)
            }
            
            # Detect memory leaks (statistically significant upward trend)
            if avg_p < 0.05 and avg_slope > avg_memory_values.mean() * 0.01:
                report['issues_detected'].append({
                    'type': 'potential_memory_leak',
                    'benchmark': benchmark,
                    'description': f'Potential memory leak detected in average memory usage',
                    'trend_slope': avg_slope,
                    'p_value': avg_p,
                    'severity': 'high' if avg_slope > avg_memory_values.mean() * 0.05 else 'medium'
                })
            
            if peak_p < 0.05 and peak_slope > peak_memory_values.mean() * 0.01:
                report['issues_detected'].append({
                    'type': 'potential_memory_leak',
                    'benchmark': benchmark,
                    'description': f'Potential memory leak detected in peak memory usage',
                    'trend_slope': peak_slope,
                    'p_value': peak_p,
                    'severity': 'high' if peak_slope > peak_memory_values.mean() * 0.05 else 'medium'
                })
        
        report['stability_metrics']['memory'] = memory_metrics

    def _analyze_threading_stability(self, data: List[Dict[str, Any]], report: Dict[str, Any]) -> None:
        """Analyze threading and concurrency stability."""
        print("🧵 Analyzing threading stability...")
        
        threading_data = []
        for item in data:
            thread_count = item.get('concurrent_threads', 1)
            timing = self._safe_get_numeric(item, 'avg_execution_time_ns', 'avg_execution_time')
            benchmark_name = item.get('benchmark_name', 'unknown')
            
            if timing > 0:
                threading_data.append({
                    'benchmark': benchmark_name,
                    'thread_count': thread_count,
                    'execution_time': timing
                })
        
        if not threading_data:
            return
        
        df = pd.DataFrame(threading_data)
        threading_metrics = {}
        
        for benchmark in df['benchmark'].unique():
            benchmark_data = df[df['benchmark'] == benchmark]
            
            # Analyze scalability by thread count
            thread_groups = benchmark_data.groupby('thread_count')['execution_time'].agg(['mean', 'std', 'count'])
            
            if len(thread_groups) > 1:
                threading_metrics[benchmark] = {
                    'thread_scalability': thread_groups.to_dict(),
                    'max_threads_tested': int(benchmark_data['thread_count'].max()),
                    'min_threads_tested': int(benchmark_data['thread_count'].min()),
                    'sample_count': len(benchmark_data)
                }
                
                # Detect poor scalability or thread safety issues
                single_thread_time = thread_groups.loc[1, 'mean'] if 1 in thread_groups.index else None
                max_thread_time = thread_groups['mean'].iloc[-1]
                
                if single_thread_time and max_thread_time > single_thread_time * 1.5:
                    report['issues_detected'].append({
                        'type': 'poor_thread_scalability',
                        'benchmark': benchmark,
                        'description': f'Poor threading scalability detected',
                        'single_thread_time': single_thread_time,
                        'max_thread_time': max_thread_time,
                        'scalability_ratio': max_thread_time / single_thread_time,
                        'severity': 'medium'
                    })
        
        report['stability_metrics']['threading'] = threading_metrics

    def _detect_stability_issues(self, data: List[Dict[str, Any]], report: Dict[str, Any]) -> None:
        """Detect various stability issues and generate recommendations."""
        issues = report['issues_detected']
        recommendations = report['recommendations']
        
        # Generate recommendations based on detected issues
        if any(issue['type'] == 'potential_memory_leak' for issue in issues):
            recommendations.append({
                'category': 'memory_management',
                'recommendation': 'Investigate potential memory leaks using memory profiling tools',
                'priority': 'high'
            })
        
        if any(issue['type'] == 'high_performance_variability' for issue in issues):
            recommendations.append({
                'category': 'performance_consistency',
                'recommendation': 'Investigate sources of performance variability (system load, thermal throttling)',
                'priority': 'medium'
            })
        
        if any(issue['type'] == 'performance_degradation' for issue in issues):
            recommendations.append({
                'category': 'performance_optimization',
                'recommendation': 'Profile degrading benchmarks to identify algorithmic inefficiencies',
                'priority': 'high'
            })
        
        if any(issue['type'] == 'poor_thread_scalability' for issue in issues):
            recommendations.append({
                'category': 'concurrency',
                'recommendation': 'Review thread synchronization and data sharing patterns',
                'priority': 'medium'
            })

    def _generate_stability_visualizations(self, data: List[Dict[str, Any]]) -> None:
        """Generate stability analysis visualizations."""
        if not data:
            return
        
        df = pd.DataFrame(data)
        
        # Performance stability over time
        if 'avg_execution_time_ns' in df.columns:
            plt.figure(figsize=(16, 10))
            
            for benchmark in df['benchmark_name'].unique()[:5]:  # Limit to 5 benchmarks
                benchmark_data = df[df['benchmark_name'] == benchmark]
                if len(benchmark_data) > 1:
                    plt.plot(range(len(benchmark_data)), 
                           benchmark_data['avg_execution_time_ns'] / 1_000_000,
                           marker='o', label=benchmark, alpha=0.7)
            
            plt.title('Performance Stability Over Time')
            plt.xlabel('Test Iteration')
            plt.ylabel('Execution Time (ms)')
            plt.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
            plt.grid(True, alpha=0.3)
            plt.tight_layout()
            plt.savefig(self.output_dir / 'performance_stability_timeline.png', 
                       dpi=300, bbox_inches='tight')
            plt.close()
        
        # Memory usage stability
        if 'avg_memory_usage' in df.columns and df['avg_memory_usage'].sum() > 0:
            plt.figure(figsize=(16, 10))
            
            for benchmark in df['benchmark_name'].unique()[:5]:
                benchmark_data = df[df['benchmark_name'] == benchmark]
                memory_data = benchmark_data['avg_memory_usage'] / (1024 * 1024)  # Convert to MB
                if len(memory_data) > 1 and memory_data.sum() > 0:
                    plt.plot(range(len(memory_data)), memory_data,
                           marker='s', label=f'{benchmark} (Memory)', alpha=0.7)
            
            plt.title('Memory Usage Stability Over Time')
            plt.xlabel('Test Iteration')
            plt.ylabel('Memory Usage (MB)')
            plt.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
            plt.grid(True, alpha=0.3)
            plt.tight_layout()
            plt.savefig(self.output_dir / 'memory_stability_timeline.png',
                       dpi=300, bbox_inches='tight')
            plt.close()

    def _generate_html_stability_report(self, report: Dict[str, Any], data: List[Dict[str, Any]]) -> None:
        """Generate comprehensive HTML stability report."""
        html_file = self.output_dir / "stability_report.html"
        
        issues_by_severity = {'high': [], 'medium': [], 'low': []}
        for issue in report['issues_detected']:
            severity = issue.get('severity', 'low')
            issues_by_severity[severity].append(issue)
        
        html_content = f"""
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>NexusSynth Stability Analysis Report</title>
    <style>
        body {{ 
            font-family: -apple-system, BlinkMacSystemFont, sans-serif;
            line-height: 1.6; margin: 0; padding: 20px; 
            background-color: #f5f5f5;
        }}
        .container {{ 
            max-width: 1200px; margin: 0 auto; background: white; 
            padding: 30px; border-radius: 10px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
        }}
        h1, h2, h3 {{ color: #333; }}
        .metric {{ 
            display: inline-block; margin: 10px; padding: 15px; 
            background: #e8f4f8; border-radius: 8px;
            border-left: 4px solid #3498db;
        }}
        .issue {{ 
            margin: 15px 0; padding: 15px; border-radius: 8px; 
            border-left: 4px solid;
        }}
        .issue-high {{ border-left-color: #dc3545; background-color: #f8d7da; }}
        .issue-medium {{ border-left-color: #ffc107; background-color: #fff3cd; }}
        .issue-low {{ border-left-color: #28a745; background-color: #d4edda; }}
        .chart {{ text-align: center; margin: 20px 0; }}
        .chart img {{ max-width: 100%; height: auto; border-radius: 4px; }}
        table {{ width: 100%; border-collapse: collapse; margin: 20px 0; }}
        th, td {{ padding: 12px; text-align: left; border-bottom: 1px solid #ddd; }}
        th {{ background-color: #f8f9fa; }}
        .recommendations {{ background: #e8f5e8; padding: 20px; border-radius: 8px; margin: 20px 0; }}
    </style>
</head>
<body>
    <div class="container">
        <h1>🔍 NexusSynth Stability Analysis Report</h1>
        <p><strong>Generated:</strong> {report['analysis_timestamp']}</p>
        
        <div class="metric">
            <h3>{report['total_data_points']}</h3>
            <p>Data Points Analyzed</p>
        </div>
        <div class="metric">
            <h3>{len(report['issues_detected'])}</h3>
            <p>Issues Detected</p>
        </div>
        <div class="metric">
            <h3>{len(issues_by_severity['high'])}</h3>
            <p>High Severity Issues</p>
        </div>
        
        <h2>🚨 Stability Issues</h2>
        """
        
        for severity in ['high', 'medium', 'low']:
            if issues_by_severity[severity]:
                html_content += f"<h3>📈 {severity.title()} Severity Issues</h3>"
                for issue in issues_by_severity[severity]:
                    html_content += f"""
                    <div class="issue issue-{severity}">
                        <h4>{issue.get('type', 'Unknown Issue').replace('_', ' ').title()}</h4>
                        <p><strong>Benchmark:</strong> {issue.get('benchmark', 'Unknown')}</p>
                        <p>{issue.get('description', 'No description available')}</p>
                    </div>
                    """
        
        if not report['issues_detected']:
            html_content += "<p>✅ No stability issues detected during long-running tests.</p>"
        
        html_content += """
        <h2>📊 Stability Visualizations</h2>
        <div class="chart">
            <h3>Performance Stability Timeline</h3>
            <img src="performance_stability_timeline.png" alt="Performance Stability Timeline">
        </div>
        <div class="chart">
            <h3>Memory Usage Stability</h3>
            <img src="memory_stability_timeline.png" alt="Memory Stability Timeline">
        </div>
        """
        
        if report['recommendations']:
            html_content += """
            <div class="recommendations">
                <h2>💡 Recommendations</h2>
                <ul>
            """
            for rec in report['recommendations']:
                html_content += f"<li><strong>{rec['category'].replace('_', ' ').title()}:</strong> {rec['recommendation']} (Priority: {rec['priority']})</li>"
            
            html_content += "</ul></div>"
        
        html_content += """
        </div>
    </body>
    </html>
        """
        
        with open(html_file, 'w') as f:
            f.write(html_content)

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

def main():
    parser = argparse.ArgumentParser(description='Analyze stability test results')
    parser.add_argument('--input', type=Path, required=True,
                       help='Input directory with stability test results')
    parser.add_argument('--output', type=Path, required=True,
                       help='Output directory for stability analysis')
    
    args = parser.parse_args()
    
    if not args.input.exists():
        print(f"❌ Input directory does not exist: {args.input}")
        return 1
    
    analyzer = StabilityAnalyzer(args.input, args.output)
    report = analyzer.analyze_stability()
    
    # Return error code if critical issues detected
    critical_issues = [issue for issue in report.get('issues_detected', []) 
                      if issue.get('severity') == 'high']
    
    if critical_issues:
        print(f"❌ {len(critical_issues)} critical stability issues detected!")
        return 1
    
    return 0

if __name__ == '__main__':
    sys.exit(main())