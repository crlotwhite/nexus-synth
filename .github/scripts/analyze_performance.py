#!/usr/bin/env python3
"""
Performance Analysis Script for NexusSynth CI/CD Pipeline

This script analyzes benchmark results from multiple platforms and generates
comprehensive performance reports with regression detection.
"""

import argparse
import json
import os
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Any
import csv
from datetime import datetime, timedelta

try:
    import pandas as pd
    import matplotlib.pyplot as plt
    import seaborn as sns
    import numpy as np
    from scipy import stats
except ImportError as e:
    print(f"❌ Missing required dependency: {e}")
    print("💡 Install with: pip install pandas matplotlib seaborn numpy scipy")
    sys.exit(1)

class PerformanceAnalyzer:
    """Analyzes benchmark results and detects performance regressions."""
    
    def __init__(self, input_dir: Path, output_dir: Path, baseline_branch: str = "main"):
        self.input_dir = input_dir
        self.output_dir = output_dir
        self.baseline_branch = baseline_branch
        self.benchmark_data = []
        self.platforms = set()
        
        # Create output directory
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
        # Configure matplotlib for headless environments
        plt.switch_backend('Agg')
        sns.set_style("whitegrid")
        plt.rcParams['figure.figsize'] = (12, 8)
        plt.rcParams['font.size'] = 10

    def load_benchmark_data(self) -> None:
        """Load all benchmark JSON files from the input directory."""
        print(f"📊 Loading benchmark data from {self.input_dir}")
        
        json_files = list(self.input_dir.rglob("*.json"))
        if not json_files:
            print("⚠️  No JSON benchmark files found")
            return
            
        for json_file in json_files:
            try:
                with open(json_file, 'r') as f:
                    data = json.load(f)
                    
                # Extract platform from directory or file name
                platform = self._extract_platform_name(json_file)
                self.platforms.add(platform)
                
                # Process benchmark results
                if isinstance(data, list):
                    for benchmark in data:
                        self._process_benchmark_entry(benchmark, platform, json_file)
                elif isinstance(data, dict) and 'results' in data:
                    for benchmark in data['results']:
                        self._process_benchmark_entry(benchmark, platform, json_file)
                else:
                    self._process_benchmark_entry(data, platform, json_file)
                        
            except (json.JSONDecodeError, KeyError) as e:
                print(f"⚠️  Failed to parse {json_file}: {e}")
                continue
                
        print(f"✅ Loaded {len(self.benchmark_data)} benchmark results from {len(self.platforms)} platforms")

    def _extract_platform_name(self, file_path: Path) -> str:
        """Extract platform name from file path."""
        path_str = str(file_path)
        
        if "Windows" in path_str or "windows" in path_str:
            return "Windows x64"
        elif "Ubuntu" in path_str and "GCC" in path_str:
            return "Ubuntu GCC"
        elif "Ubuntu" in path_str and "Clang" in path_str:
            return "Ubuntu Clang"
        elif "macOS-13" in path_str or "macos-13" in path_str:
            return "macOS Intel"
        elif "macOS-14" in path_str or "macos-14" in path_str:
            return "macOS ARM64"
        else:
            return "Unknown Platform"

    def _process_benchmark_entry(self, benchmark: Dict[str, Any], platform: str, source_file: Path) -> None:
        """Process a single benchmark entry."""
        try:
            entry = {
                'benchmark_name': benchmark.get('benchmark_name', 'unknown'),
                'platform': platform,
                'avg_execution_time_ns': self._safe_get_numeric(benchmark, 'avg_execution_time', 'avg_execution_time_ns'),
                'min_execution_time_ns': self._safe_get_numeric(benchmark, 'min_execution_time', 'min_execution_time_ns'),
                'max_execution_time_ns': self._safe_get_numeric(benchmark, 'max_execution_time', 'max_execution_time_ns'),
                'avg_memory_usage': benchmark.get('avg_memory_usage', 0),
                'peak_allocation': benchmark.get('peak_allocation', 0),
                'formant_preservation_score': benchmark.get('formant_preservation_score', 0.0),
                'benchmark_successful': benchmark.get('benchmark_successful', True),
                'concurrent_threads': benchmark.get('concurrent_threads', 1),
                'source_file': str(source_file),
                'timestamp': datetime.now().isoformat()
            }
            
            # Only add successful benchmarks with valid timing data
            if entry['benchmark_successful'] and entry['avg_execution_time_ns'] > 0:
                self.benchmark_data.append(entry)
                
        except (KeyError, ValueError, TypeError) as e:
            print(f"⚠️  Skipping invalid benchmark entry: {e}")

    def _safe_get_numeric(self, data: Dict[str, Any], *keys) -> float:
        """Safely extract numeric value from various possible keys."""
        for key in keys:
            if key in data:
                value = data[key]
                if isinstance(value, (int, float)):
                    return float(value)
                elif isinstance(value, str) and value.isdigit():
                    return float(value)
        return 0.0

    def generate_performance_report(self) -> None:
        """Generate comprehensive performance analysis report."""
        if not self.benchmark_data:
            print("❌ No benchmark data to analyze")
            return
            
        print("📈 Generating performance analysis report...")
        
        df = pd.DataFrame(self.benchmark_data)
        
        # Generate summary statistics
        self._generate_summary_report(df)
        
        # Generate platform comparison charts
        self._generate_platform_comparison_charts(df)
        
        # Generate benchmark performance charts
        self._generate_benchmark_performance_charts(df)
        
        # Generate regression analysis
        self._generate_regression_analysis(df)
        
        # Generate HTML report
        self._generate_html_report(df)
        
        print(f"✅ Performance report generated in {self.output_dir}")

    def _generate_summary_report(self, df: pd.DataFrame) -> None:
        """Generate summary statistics and markdown report."""
        summary_file = self.output_dir / "summary.md"
        
        with open(summary_file, 'w') as f:
            f.write("## 📊 Performance Summary\n\n")
            
            # Overall statistics
            f.write("### Overall Statistics\n")
            f.write(f"- **Total Benchmarks**: {len(df)}\n")
            f.write(f"- **Platforms Tested**: {', '.join(sorted(self.platforms))}\n")
            f.write(f"- **Unique Benchmark Types**: {df['benchmark_name'].nunique()}\n\n")
            
            # Performance by platform
            f.write("### Performance by Platform\n")
            platform_stats = df.groupby('platform').agg({
                'avg_execution_time_ns': ['mean', 'std', 'min', 'max'],
                'avg_memory_usage': ['mean', 'std'],
                'formant_preservation_score': 'mean'
            }).round(2)
            
            f.write("| Platform | Avg Time (ms) | Memory (MB) | Quality Score |\n")
            f.write("|----------|---------------|-------------|---------------|\n")
            
            for platform in platform_stats.index:
                avg_time_ms = platform_stats.loc[platform, ('avg_execution_time_ns', 'mean')] / 1_000_000
                avg_memory_mb = platform_stats.loc[platform, ('avg_memory_usage', 'mean')] / (1024 * 1024)
                quality_score = platform_stats.loc[platform, ('formant_preservation_score', 'mean')]
                
                f.write(f"| {platform} | {avg_time_ms:.2f} | {avg_memory_mb:.2f} | {quality_score:.3f} |\n")
            
            f.write("\n")
            
            # Top performing benchmarks
            f.write("### 🏆 Top Performing Benchmarks\n")
            fastest_benchmarks = df.nsmallest(5, 'avg_execution_time_ns')[['benchmark_name', 'platform', 'avg_execution_time_ns']]
            
            for _, row in fastest_benchmarks.iterrows():
                time_ms = row['avg_execution_time_ns'] / 1_000_000
                f.write(f"- **{row['benchmark_name']}** on {row['platform']}: {time_ms:.2f}ms\n")
            
            f.write("\n")
            
            # Performance concerns (if any)
            slow_benchmarks = df[df['avg_execution_time_ns'] > df['avg_execution_time_ns'].quantile(0.9)]
            if not slow_benchmarks.empty:
                f.write("### ⚠️ Performance Concerns\n")
                f.write("Benchmarks in the slowest 10%:\n")
                
                for _, row in slow_benchmarks.iterrows():
                    time_ms = row['avg_execution_time_ns'] / 1_000_000
                    f.write(f"- **{row['benchmark_name']}** on {row['platform']}: {time_ms:.2f}ms\n")

    def _generate_platform_comparison_charts(self, df: pd.DataFrame) -> None:
        """Generate charts comparing performance across platforms."""
        # Execution time comparison
        plt.figure(figsize=(14, 8))
        sns.boxplot(data=df, x='platform', y='avg_execution_time_ns', hue='benchmark_name')
        plt.title('Execution Time Distribution by Platform')
        plt.ylabel('Average Execution Time (nanoseconds)')
        plt.xlabel('Platform')
        plt.yscale('log')  # Log scale for better readability
        plt.xticks(rotation=45)
        plt.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
        plt.tight_layout()
        plt.savefig(self.output_dir / 'platform_execution_time_comparison.png', dpi=300, bbox_inches='tight')
        plt.close()
        
        # Memory usage comparison
        if df['avg_memory_usage'].sum() > 0:
            plt.figure(figsize=(12, 6))
            memory_stats = df.groupby(['platform', 'benchmark_name'])['avg_memory_usage'].mean().unstack()
            memory_stats.plot(kind='bar', stacked=True)
            plt.title('Memory Usage by Platform and Benchmark')
            plt.ylabel('Average Memory Usage (bytes)')
            plt.xlabel('Platform')
            plt.xticks(rotation=45)
            plt.legend(title='Benchmark', bbox_to_anchor=(1.05, 1), loc='upper left')
            plt.tight_layout()
            plt.savefig(self.output_dir / 'platform_memory_comparison.png', dpi=300, bbox_inches='tight')
            plt.close()

    def _generate_benchmark_performance_charts(self, df: pd.DataFrame) -> None:
        """Generate charts showing individual benchmark performance."""
        unique_benchmarks = df['benchmark_name'].unique()
        
        for benchmark in unique_benchmarks:
            benchmark_data = df[df['benchmark_name'] == benchmark]
            if len(benchmark_data) < 2:
                continue
                
            plt.figure(figsize=(10, 6))
            sns.barplot(data=benchmark_data, x='platform', y='avg_execution_time_ns', 
                       palette='viridis', errorbar='sd')
            plt.title(f'Performance: {benchmark}')
            plt.ylabel('Average Execution Time (nanoseconds)')
            plt.xlabel('Platform')
            plt.yscale('log')
            plt.xticks(rotation=45)
            
            # Add value labels on bars
            ax = plt.gca()
            for p in ax.patches:
                if p.get_height() > 0:
                    ax.annotate(f'{p.get_height()/1000:.1f}μs', 
                               (p.get_x() + p.get_width()/2., p.get_height()),
                               ha='center', va='bottom', fontsize=8)
            
            plt.tight_layout()
            safe_name = benchmark.replace(' ', '_').replace('/', '_')
            plt.savefig(self.output_dir / f'benchmark_{safe_name}_performance.png', 
                       dpi=300, bbox_inches='tight')
            plt.close()

    def _generate_regression_analysis(self, df: pd.DataFrame) -> None:
        """Generate regression analysis comparing with baseline."""
        regression_file = self.output_dir / "regression_analysis.json"
        
        # For now, generate basic statistical analysis
        # In a real implementation, this would compare with historical data
        regression_data = {
            'analysis_timestamp': datetime.now().isoformat(),
            'baseline_branch': self.baseline_branch,
            'performance_metrics': {},
            'regressions_detected': [],
            'improvements_detected': []
        }
        
        # Calculate coefficient of variation for each benchmark
        for benchmark in df['benchmark_name'].unique():
            benchmark_data = df[df['benchmark_name'] == benchmark]['avg_execution_time_ns']
            
            cv = benchmark_data.std() / benchmark_data.mean() if benchmark_data.mean() > 0 else 0
            regression_data['performance_metrics'][benchmark] = {
                'mean_execution_time_ns': float(benchmark_data.mean()),
                'std_execution_time_ns': float(benchmark_data.std()),
                'coefficient_of_variation': float(cv),
                'platform_count': len(benchmark_data),
                'consistency_score': float(1.0 - cv) if cv < 1.0 else 0.0
            }
            
            # Flag high variability as potential concern
            if cv > 0.3:  # 30% coefficient of variation threshold
                regression_data['regressions_detected'].append({
                    'benchmark_name': benchmark,
                    'issue': 'High performance variability across platforms',
                    'coefficient_of_variation': float(cv),
                    'recommendation': 'Investigate platform-specific performance bottlenecks'
                })
        
        with open(regression_file, 'w') as f:
            json.dump(regression_data, f, indent=2)

    def _generate_html_report(self, df: pd.DataFrame) -> None:
        """Generate comprehensive HTML report."""
        html_file = self.output_dir / "performance_report.html"
        
        html_content = f"""
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>NexusSynth Performance Report</title>
    <style>
        body {{ 
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
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
        .metric {{ 
            display: inline-block; 
            margin: 10px; 
            padding: 15px; 
            background: #e8f4f8; 
            border-radius: 8px;
            border-left: 4px solid #3498db;
        }}
        .chart {{ 
            text-align: center; 
            margin: 20px 0; 
            padding: 20px;
            background: #fafafa;
            border-radius: 8px;
        }}
        .chart img {{ 
            max-width: 100%; 
            height: auto;
            border-radius: 4px;
            box-shadow: 0 2px 8px rgba(0,0,0,0.1);
        }}
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
            background-color: #3498db; 
            color: white;
        }}
        .footer {{ 
            margin-top: 40px; 
            padding-top: 20px; 
            border-top: 1px solid #ddd;
            text-align: center;
            color: #666;
        }}
    </style>
</head>
<body>
    <div class="container">
        <h1>🚀 NexusSynth Performance Report</h1>
        <p><strong>Generated:</strong> {datetime.now().strftime('%Y-%m-%d %H:%M:%S UTC')}</p>
        <p><strong>Baseline Branch:</strong> {self.baseline_branch}</p>
        
        <h2>📊 Key Metrics</h2>
        <div class="metric">
            <h3>{len(df)}</h3>
            <p>Total Benchmarks</p>
        </div>
        <div class="metric">
            <h3>{len(self.platforms)}</h3>
            <p>Platforms Tested</p>
        </div>
        <div class="metric">
            <h3>{df['benchmark_name'].nunique()}</h3>
            <p>Benchmark Types</p>
        </div>
        <div class="metric">
            <h3>{df['avg_execution_time_ns'].mean()/1000000:.2f}ms</h3>
            <p>Average Execution Time</p>
        </div>
        
        <h2>📈 Performance Charts</h2>
        <div class="chart">
            <h3>Execution Time by Platform</h3>
            <img src="platform_execution_time_comparison.png" alt="Platform Execution Time Comparison">
        </div>
        
        {"<div class='chart'><h3>Memory Usage by Platform</h3><img src='platform_memory_comparison.png' alt='Platform Memory Comparison'></div>" if df['avg_memory_usage'].sum() > 0 else ""}
        
        <h2>📋 Detailed Results</h2>
        <table>
            <thead>
                <tr>
                    <th>Benchmark</th>
                    <th>Platform</th>
                    <th>Avg Time (ms)</th>
                    <th>Memory (MB)</th>
                    <th>Quality Score</th>
                </tr>
            </thead>
            <tbody>
        """
        
        for _, row in df.iterrows():
            time_ms = row['avg_execution_time_ns'] / 1_000_000
            memory_mb = row['avg_memory_usage'] / (1024 * 1024)
            quality = row['formant_preservation_score']
            
            html_content += f"""
                <tr>
                    <td>{row['benchmark_name']}</td>
                    <td>{row['platform']}</td>
                    <td>{time_ms:.2f}</td>
                    <td>{memory_mb:.2f}</td>
                    <td>{quality:.3f}</td>
                </tr>
            """
        
        html_content += """
            </tbody>
        </table>
        
        <div class="footer">
            <p>Generated by NexusSynth CI/CD Performance Analysis</p>
        </div>
    </div>
</body>
</html>
        """
        
        with open(html_file, 'w') as f:
            f.write(html_content)


def main():
    parser = argparse.ArgumentParser(description='Analyze NexusSynth performance benchmarks')
    parser.add_argument('--input', type=Path, required=True,
                       help='Input directory containing benchmark results')
    parser.add_argument('--output', type=Path, required=True,
                       help='Output directory for analysis results')
    parser.add_argument('--baseline-branch', default='main',
                       help='Baseline branch for regression analysis')
    
    args = parser.parse_args()
    
    if not args.input.exists():
        print(f"❌ Input directory does not exist: {args.input}")
        return 1
    
    analyzer = PerformanceAnalyzer(args.input, args.output, args.baseline_branch)
    analyzer.load_benchmark_data()
    analyzer.generate_performance_report()
    
    return 0

if __name__ == '__main__':
    sys.exit(main())