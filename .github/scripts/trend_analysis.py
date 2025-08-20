#!/usr/bin/env python3
"""
Performance Trend Analysis Script

Analyzes performance trends over time and generates trend reports.
"""

import argparse
import json
import os
import sys
from pathlib import Path
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Tuple, Any

try:
    import pandas as pd
    import matplotlib.pyplot as plt
    import numpy as np
    from scipy import stats
except ImportError as e:
    print(f"❌ Missing required dependency: {e}")
    sys.exit(1)

class TrendAnalyzer:
    """Analyzes performance trends over time."""
    
    def __init__(self, current_dir: Path, output_dir: Path, platform: str):
        self.current_dir = current_dir
        self.output_dir = output_dir
        self.platform = platform
        
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
        # Configure matplotlib
        plt.switch_backend('Agg')
        plt.rcParams['figure.figsize'] = (12, 8)

    def analyze_trends(self) -> None:
        """Perform trend analysis on current benchmark data."""
        print(f"📊 Analyzing trends for platform: {self.platform}")
        
        # Load current benchmark data
        current_data = self._load_benchmark_data(self.current_dir)
        if not current_data:
            print("❌ No current benchmark data found")
            return
            
        # Generate trend analysis
        self._generate_trend_report(current_data)
        
        print(f"✅ Trend analysis completed: {self.output_dir}")

    def _load_benchmark_data(self, data_dir: Path) -> List[Dict[str, Any]]:
        """Load benchmark data from JSON files."""
        benchmark_data = []
        
        json_files = list(data_dir.rglob("*.json"))
        for json_file in json_files:
            try:
                with open(json_file, 'r') as f:
                    data = json.load(f)
                    
                if isinstance(data, list):
                    benchmark_data.extend(data)
                elif isinstance(data, dict) and 'results' in data:
                    benchmark_data.extend(data['results'])
                else:
                    benchmark_data.append(data)
                    
            except (json.JSONDecodeError, KeyError):
                continue
                
        return benchmark_data

    def _generate_trend_report(self, current_data: List[Dict[str, Any]]) -> None:
        """Generate trend analysis report."""
        df = pd.DataFrame(current_data)
        
        if df.empty:
            print("⚠️ No valid benchmark data for trend analysis")
            return
            
        # Add timestamp
        df['timestamp'] = datetime.now()
        
        # Generate summary report
        summary = {
            'platform': self.platform,
            'analysis_timestamp': datetime.now().isoformat(),
            'total_benchmarks': len(df),
            'unique_benchmark_types': df['benchmark_name'].nunique() if 'benchmark_name' in df.columns else 0,
            'performance_summary': {}
        }
        
        if 'benchmark_name' in df.columns and 'avg_execution_time_ns' in df.columns:
            # Calculate performance statistics per benchmark
            for benchmark in df['benchmark_name'].unique():
                benchmark_data = df[df['benchmark_name'] == benchmark]['avg_execution_time_ns']
                
                summary['performance_summary'][benchmark] = {
                    'mean_time_ms': float(benchmark_data.mean() / 1_000_000),
                    'std_time_ms': float(benchmark_data.std() / 1_000_000),
                    'min_time_ms': float(benchmark_data.min() / 1_000_000),
                    'max_time_ms': float(benchmark_data.max() / 1_000_000),
                    'sample_count': len(benchmark_data)
                }
        
        # Save summary report
        summary_file = self.output_dir / f"trend_summary_{self.platform.replace(' ', '_')}.json"
        with open(summary_file, 'w') as f:
            json.dump(summary, f, indent=2)
            
        # Generate trend charts
        self._generate_trend_charts(df)

    def _generate_trend_charts(self, df: pd.DataFrame) -> None:
        """Generate trend visualization charts."""
        if 'benchmark_name' not in df.columns or 'avg_execution_time_ns' not in df.columns:
            return
            
        # Performance distribution chart
        plt.figure(figsize=(12, 8))
        
        unique_benchmarks = df['benchmark_name'].unique()
        colors = plt.cm.Set3(np.linspace(0, 1, len(unique_benchmarks)))
        
        for i, benchmark in enumerate(unique_benchmarks):
            benchmark_data = df[df['benchmark_name'] == benchmark]['avg_execution_time_ns'] / 1_000_000
            
            plt.hist(benchmark_data, bins=20, alpha=0.7, 
                    label=benchmark, color=colors[i])
        
        plt.xlabel('Execution Time (ms)')
        plt.ylabel('Frequency')
        plt.title(f'Performance Distribution - {self.platform}')
        plt.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
        plt.tight_layout()
        
        chart_file = self.output_dir / f"performance_distribution_{self.platform.replace(' ', '_')}.png"
        plt.savefig(chart_file, dpi=300, bbox_inches='tight')
        plt.close()
        
        # Box plot comparison
        plt.figure(figsize=(14, 8))
        
        performance_data = []
        benchmark_labels = []
        
        for benchmark in unique_benchmarks:
            benchmark_data = df[df['benchmark_name'] == benchmark]['avg_execution_time_ns'] / 1_000_000
            performance_data.append(benchmark_data.tolist())
            benchmark_labels.append(benchmark)
        
        plt.boxplot(performance_data, labels=benchmark_labels, patch_artist=True,
                   boxprops=dict(facecolor='lightblue', alpha=0.7))
        
        plt.ylabel('Execution Time (ms)')
        plt.title(f'Performance Comparison - {self.platform}')
        plt.xticks(rotation=45, ha='right')
        plt.yscale('log')
        plt.tight_layout()
        
        chart_file = self.output_dir / f"performance_comparison_{self.platform.replace(' ', '_')}.png"
        plt.savefig(chart_file, dpi=300, bbox_inches='tight')
        plt.close()

def main():
    parser = argparse.ArgumentParser(description='Analyze performance trends')
    parser.add_argument('--current', type=Path, required=True,
                       help='Directory with current benchmark results')
    parser.add_argument('--output', type=Path, required=True,
                       help='Output directory for trend analysis')
    parser.add_argument('--platform', required=True,
                       help='Platform name for analysis')
    
    args = parser.parse_args()
    
    if not args.current.exists():
        print(f"❌ Current data directory does not exist: {args.current}")
        return 1
    
    analyzer = TrendAnalyzer(args.current, args.output, args.platform)
    analyzer.analyze_trends()
    
    return 0

if __name__ == '__main__':
    sys.exit(main())