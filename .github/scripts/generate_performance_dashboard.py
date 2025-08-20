#!/usr/bin/env python3
"""
Performance Dashboard Generator for NexusSynth

This script generates comprehensive performance dashboards and visualizations
from the performance regression database.
"""

import argparse
import sqlite3
import json
import os
from pathlib import Path
from datetime import datetime, timedelta
from typing import Dict, List, Optional, Tuple

try:
    import pandas as pd
    import matplotlib.pyplot as plt
    import seaborn as sns
    import numpy as np
    import plotly.graph_objects as go
    import plotly.express as px
    from plotly.subplots import make_subplots
    import plotly.offline as pyo
except ImportError as e:
    print(f"❌ Missing required dependency: {e}")
    print("💡 Install with: pip install pandas matplotlib seaborn numpy plotly")
    exit(1)


class PerformanceDashboard:
    """Generates interactive performance dashboards."""
    
    def __init__(self, db_path: Path, output_dir: Path):
        self.db_path = db_path
        self.output_dir = output_dir
        self.output_dir.mkdir(parents=True, exist_ok=True)
        
        # Configure plotting styles
        plt.style.use('default')
        sns.set_palette("husl")
        
        # Configure plotly theme
        self.plotly_template = 'plotly_white'
    
    def generate_dashboard(self, days_back: int = 90, 
                          include_trends: bool = True,
                          include_platform_comparison: bool = True,
                          include_regression_history: bool = True):
        """Generate complete performance dashboard."""
        print(f"📊 Generating performance dashboard for last {days_back} days...")
        
        # Load data from database
        metrics_df = self._load_metrics_data(days_back)
        alerts_df = self._load_alerts_data(days_back)
        
        if metrics_df.empty:
            print("⚠️ No performance data found in database")
            self._generate_empty_dashboard()
            return
        
        print(f"✅ Loaded {len(metrics_df)} metrics and {len(alerts_df)} alerts")
        
        # Generate individual dashboard components
        dashboard_components = []
        
        if include_trends:
            dashboard_components.append(self._generate_performance_trends(metrics_df))
        
        if include_platform_comparison:
            dashboard_components.append(self._generate_platform_comparison(metrics_df))
        
        if include_regression_history:
            dashboard_components.append(self._generate_regression_history(alerts_df))
        
        dashboard_components.append(self._generate_summary_statistics(metrics_df, alerts_df))
        
        # Generate main dashboard HTML
        self._generate_main_dashboard(dashboard_components, metrics_df, alerts_df)
        
        # Generate additional static reports
        self._generate_static_reports(metrics_df, alerts_df)
        
        print(f"✅ Dashboard generated at: {self.output_dir}")
    
    def _load_metrics_data(self, days_back: int) -> pd.DataFrame:
        """Load performance metrics from database."""
        cutoff_date = (datetime.now() - timedelta(days=days_back)).isoformat()
        
        try:
            with sqlite3.connect(self.db_path) as conn:
                query = """
                SELECT benchmark_name, platform, execution_time_ns, memory_usage,
                       quality_score, timestamp, commit_hash, branch, pr_number
                FROM performance_metrics
                WHERE timestamp > ?
                ORDER BY timestamp ASC
                """
                
                df = pd.read_sql_query(query, conn, params=[cutoff_date])
                df['timestamp'] = pd.to_datetime(df['timestamp'])
                df['execution_time_ms'] = df['execution_time_ns'] / 1_000_000
                df['memory_usage_mb'] = df['memory_usage'] / (1024 * 1024)
                
                return df
                
        except sqlite3.Error as e:
            print(f"❌ Database error: {e}")
            return pd.DataFrame()
    
    def _load_alerts_data(self, days_back: int) -> pd.DataFrame:
        """Load regression alerts from database."""
        cutoff_date = (datetime.now() - timedelta(days=days_back)).isoformat()
        
        try:
            with sqlite3.connect(self.db_path) as conn:
                query = """
                SELECT benchmark_name, platform, severity, current_value, baseline_value,
                       change_percent, timestamp, commit_hash, statistical_significance
                FROM regression_alerts
                WHERE created_at > ?
                ORDER BY created_at ASC
                """
                
                df = pd.read_sql_query(query, conn, params=[cutoff_date])
                if not df.empty:
                    df['timestamp'] = pd.to_datetime(df['timestamp'])
                    df['current_value_ms'] = df['current_value'] / 1_000_000
                    df['baseline_value_ms'] = df['baseline_value'] / 1_000_000
                
                return df
                
        except sqlite3.Error as e:
            print(f"❌ Database error: {e}")
            return pd.DataFrame()
    
    def _generate_performance_trends(self, df: pd.DataFrame) -> str:
        """Generate performance trends charts."""
        print("📈 Generating performance trends...")
        
        # Create subplot figure
        unique_benchmarks = df['benchmark_name'].unique()[:6]  # Limit to top 6 benchmarks
        
        fig = make_subplots(
            rows=2, cols=3,
            subplot_titles=[f"Trend: {bench[:20]}..." if len(bench) > 20 else f"Trend: {bench}" 
                           for bench in unique_benchmarks[:6]],
            vertical_spacing=0.12,
            horizontal_spacing=0.1
        )
        
        colors = px.colors.qualitative.Set3
        
        for i, benchmark in enumerate(unique_benchmarks):
            benchmark_data = df[df['benchmark_name'] == benchmark]
            
            row = (i // 3) + 1
            col = (i % 3) + 1
            
            for j, platform in enumerate(benchmark_data['platform'].unique()):
                platform_data = benchmark_data[benchmark_data['platform'] == platform]
                
                # Rolling average for smoother trends
                platform_data = platform_data.sort_values('timestamp')
                platform_data['rolling_avg'] = platform_data['execution_time_ms'].rolling(window=5, min_periods=1).mean()
                
                fig.add_trace(
                    go.Scatter(
                        x=platform_data['timestamp'],
                        y=platform_data['rolling_avg'],
                        name=f"{platform}",
                        line=dict(color=colors[j % len(colors)]),
                        showlegend=(i == 0)  # Only show legend for first subplot
                    ),
                    row=row, col=col
                )
        
        fig.update_layout(
            title="Performance Trends Over Time (5-point rolling average)",
            height=800,
            template=self.plotly_template,
            font=dict(size=10)
        )
        
        fig.update_yaxes(title_text="Execution Time (ms)")
        
        # Save as HTML
        trends_path = self.output_dir / "performance_trends.html"
        fig.write_html(str(trends_path))
        
        return f'<iframe src="performance_trends.html" width="100%" height="850" frameborder="0"></iframe>'
    
    def _generate_platform_comparison(self, df: pd.DataFrame) -> str:
        """Generate platform performance comparison charts."""
        print("🖥️ Generating platform comparisons...")
        
        # Calculate platform statistics
        platform_stats = df.groupby(['benchmark_name', 'platform']).agg({
            'execution_time_ms': ['mean', 'std', 'min', 'max'],
            'memory_usage_mb': 'mean',
            'quality_score': 'mean'
        }).reset_index()
        
        # Flatten column names
        platform_stats.columns = [
            'benchmark_name', 'platform', 'avg_time', 'std_time', 'min_time', 'max_time',
            'avg_memory', 'avg_quality'
        ]
        
        # Create comparison chart
        fig = px.bar(
            platform_stats, 
            x='benchmark_name', 
            y='avg_time',
            color='platform',
            error_y='std_time',
            title="Average Execution Time by Platform and Benchmark",
            labels={
                'avg_time': 'Average Execution Time (ms)',
                'benchmark_name': 'Benchmark',
                'platform': 'Platform'
            },
            template=self.plotly_template
        )
        
        fig.update_layout(
            xaxis_tickangle=-45,
            height=600,
            legend=dict(orientation="h", yanchor="bottom", y=1.02, xanchor="right", x=1)
        )
        
        # Save comparison chart
        comparison_path = self.output_dir / "platform_comparison.html"
        fig.write_html(str(comparison_path))
        
        return f'<iframe src="platform_comparison.html" width="100%" height="650" frameborder="0"></iframe>'
    
    def _generate_regression_history(self, alerts_df: pd.DataFrame) -> str:
        """Generate regression history visualization."""
        print("🚨 Generating regression history...")
        
        if alerts_df.empty:
            return '<div class="alert alert-info">No performance regressions detected in the selected time period.</div>'
        
        # Create timeline of regressions
        fig = go.Figure()
        
        severity_colors = {
            'minor': '#28a745',      # Green
            'moderate': '#ffc107',   # Yellow  
            'major': '#fd7e14',      # Orange
            'critical': '#dc3545'    # Red
        }
        
        for severity in ['critical', 'major', 'moderate', 'minor']:
            severity_data = alerts_df[alerts_df['severity'] == severity]
            if severity_data.empty:
                continue
                
            fig.add_trace(go.Scatter(
                x=severity_data['timestamp'],
                y=severity_data['change_percent'],
                mode='markers',
                name=severity.title(),
                marker=dict(
                    color=severity_colors[severity],
                    size=12,
                    symbol='diamond' if severity in ['critical', 'major'] else 'circle'
                ),
                text=[f"{row['benchmark_name']}<br>{row['platform']}<br>{row['change_percent']:.1f}% regression" 
                      for _, row in severity_data.iterrows()],
                hovertemplate="<b>%{text}</b><br>Time: %{x}<extra></extra>"
            ))
        
        fig.update_layout(
            title="Performance Regression Timeline",
            xaxis_title="Date",
            yaxis_title="Performance Change (%)",
            template=self.plotly_template,
            height=500,
            hovermode='closest'
        )
        
        fig.add_hline(y=0, line_dash="dash", line_color="gray", annotation_text="Baseline")
        
        # Save regression timeline
        regression_path = self.output_dir / "regression_timeline.html"
        fig.write_html(str(regression_path))
        
        return f'<iframe src="regression_timeline.html" width="100%" height="550" frameborder="0"></iframe>'
    
    def _generate_summary_statistics(self, metrics_df: pd.DataFrame, alerts_df: pd.DataFrame) -> str:
        """Generate summary statistics section."""
        print("📊 Generating summary statistics...")
        
        # Calculate key metrics
        total_benchmarks = len(metrics_df)
        unique_benchmarks = metrics_df['benchmark_name'].nunique()
        platforms_tested = metrics_df['platform'].nunique()
        total_regressions = len(alerts_df)
        
        avg_execution_time = metrics_df['execution_time_ms'].mean()
        avg_memory_usage = metrics_df['memory_usage_mb'].mean()
        avg_quality_score = metrics_df['quality_score'].mean()
        
        # Recent performance trend
        if len(metrics_df) > 10:
            recent_metrics = metrics_df.tail(10)
            older_metrics = metrics_df.head(10)
            
            recent_avg = recent_metrics['execution_time_ms'].mean()
            older_avg = older_metrics['execution_time_ms'].mean()
            
            trend_change = ((recent_avg - older_avg) / older_avg) * 100 if older_avg > 0 else 0
            trend_direction = "↗️" if trend_change > 2 else "↘️" if trend_change < -2 else "➡️"
        else:
            trend_change = 0
            trend_direction = "➡️"
        
        # Regression breakdown
        regression_counts = alerts_df['severity'].value_counts() if not alerts_df.empty else {}
        
        html_content = f"""
        <div class="row mb-4">
            <div class="col-md-3">
                <div class="card text-center bg-primary text-white">
                    <div class="card-body">
                        <h5 class="card-title">{total_benchmarks:,}</h5>
                        <p class="card-text">Total Measurements</p>
                    </div>
                </div>
            </div>
            <div class="col-md-3">
                <div class="card text-center bg-info text-white">
                    <div class="card-body">
                        <h5 class="card-title">{unique_benchmarks}</h5>
                        <p class="card-text">Unique Benchmarks</p>
                    </div>
                </div>
            </div>
            <div class="col-md-3">
                <div class="card text-center bg-success text-white">
                    <div class="card-body">
                        <h5 class="card-title">{platforms_tested}</h5>
                        <p class="card-text">Platforms Tested</p>
                    </div>
                </div>
            </div>
            <div class="col-md-3">
                <div class="card text-center bg-warning text-dark">
                    <div class="card-body">
                        <h5 class="card-title">{total_regressions}</h5>
                        <p class="card-text">Total Regressions</p>
                    </div>
                </div>
            </div>
        </div>
        
        <div class="row mb-4">
            <div class="col-md-4">
                <div class="card">
                    <div class="card-header">
                        <h6 class="mb-0">Average Performance</h6>
                    </div>
                    <div class="card-body">
                        <p><strong>Execution Time:</strong> {avg_execution_time:.2f} ms</p>
                        <p><strong>Memory Usage:</strong> {avg_memory_usage:.2f} MB</p>
                        <p><strong>Quality Score:</strong> {avg_quality_score:.3f}</p>
                    </div>
                </div>
            </div>
            <div class="col-md-4">
                <div class="card">
                    <div class="card-header">
                        <h6 class="mb-0">Performance Trend</h6>
                    </div>
                    <div class="card-body">
                        <p class="mb-1"><strong>Recent Change:</strong></p>
                        <p class="h5 text-{'success' if trend_change < 0 else 'danger' if trend_change > 5 else 'warning'}">
                            {trend_direction} {abs(trend_change):.1f}%
                        </p>
                        <small class="text-muted">Compared to baseline period</small>
                    </div>
                </div>
            </div>
            <div class="col-md-4">
                <div class="card">
                    <div class="card-header">
                        <h6 class="mb-0">Regression Breakdown</h6>
                    </div>
                    <div class="card-body">
                        <p><span class="badge bg-danger">Critical:</span> {regression_counts.get('critical', 0)}</p>
                        <p><span class="badge bg-warning">Major:</span> {regression_counts.get('major', 0)}</p>
                        <p><span class="badge bg-info">Moderate:</span> {regression_counts.get('moderate', 0)}</p>
                        <p><span class="badge bg-success">Minor:</span> {regression_counts.get('minor', 0)}</p>
                    </div>
                </div>
            </div>
        </div>
        """
        
        return html_content
    
    def _generate_main_dashboard(self, components: List[str], 
                               metrics_df: pd.DataFrame, alerts_df: pd.DataFrame):
        """Generate the main dashboard HTML file."""
        print("🏠 Generating main dashboard...")
        
        # Get latest data timestamp
        latest_timestamp = metrics_df['timestamp'].max().strftime('%Y-%m-%d %H:%M:%S UTC') if not metrics_df.empty else 'No data'
        
        html_content = f"""
        <!DOCTYPE html>
        <html lang="en">
        <head>
            <meta charset="UTF-8">
            <meta name="viewport" content="width=device-width, initial-scale=1.0">
            <title>NexusSynth Performance Dashboard</title>
            <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.1.3/dist/css/bootstrap.min.css" rel="stylesheet">
            <link href="https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.0.0/css/all.min.css" rel="stylesheet">
            <style>
                body {{
                    background-color: #f8f9fa;
                }}
                .dashboard-header {{
                    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
                    color: white;
                    padding: 2rem 0;
                    margin-bottom: 2rem;
                }}
                .card {{
                    border: none;
                    border-radius: 10px;
                    box-shadow: 0 0.125rem 0.25rem rgba(0, 0, 0, 0.075);
                    margin-bottom: 1.5rem;
                }}
                .alert {{
                    border-radius: 8px;
                }}
                iframe {{
                    border-radius: 8px;
                }}
                .nav-tabs .nav-link {{
                    border-radius: 8px 8px 0 0;
                }}
                .nav-tabs .nav-link.active {{
                    background-color: #667eea;
                    color: white;
                }}
                .footer {{
                    background-color: #343a40;
                    color: white;
                    text-align: center;
                    padding: 1rem 0;
                    margin-top: 3rem;
                }}
            </style>
        </head>
        <body>
            <div class="dashboard-header">
                <div class="container">
                    <div class="row">
                        <div class="col-md-8">
                            <h1 class="display-4">
                                <i class="fas fa-chart-line"></i> NexusSynth Performance Dashboard
                            </h1>
                            <p class="lead">Real-time performance monitoring and regression detection</p>
                        </div>
                        <div class="col-md-4 text-end">
                            <div class="mt-3">
                                <p class="mb-1"><strong>Last Updated:</strong></p>
                                <p class="h6">{latest_timestamp}</p>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
            
            <div class="container">
                <!-- Summary Statistics -->
                <div class="card">
                    <div class="card-header">
                        <h5 class="mb-0"><i class="fas fa-chart-bar"></i> Performance Overview</h5>
                    </div>
                    <div class="card-body">
                        {components[-1] if components else '<p>No performance data available.</p>'}
                    </div>
                </div>
                
                <!-- Tabbed Content -->
                <div class="card">
                    <div class="card-header">
                        <ul class="nav nav-tabs" id="dashboardTabs" role="tablist">
                            <li class="nav-item" role="presentation">
                                <button class="nav-link active" id="trends-tab" data-bs-toggle="tab" data-bs-target="#trends" type="button" role="tab">
                                    <i class="fas fa-chart-line"></i> Performance Trends
                                </button>
                            </li>
                            <li class="nav-item" role="presentation">
                                <button class="nav-link" id="platforms-tab" data-bs-toggle="tab" data-bs-target="#platforms" type="button" role="tab">
                                    <i class="fas fa-server"></i> Platform Comparison
                                </button>
                            </li>
                            <li class="nav-item" role="presentation">
                                <button class="nav-link" id="regressions-tab" data-bs-toggle="tab" data-bs-target="#regressions" type="button" role="tab">
                                    <i class="fas fa-exclamation-triangle"></i> Regression History
                                </button>
                            </li>
                        </ul>
                    </div>
                    <div class="card-body">
                        <div class="tab-content" id="dashboardTabsContent">
                            <div class="tab-pane fade show active" id="trends" role="tabpanel">
                                {components[0] if len(components) > 0 else '<p>No trend data available.</p>'}
                            </div>
                            <div class="tab-pane fade" id="platforms" role="tabpanel">
                                {components[1] if len(components) > 1 else '<p>No platform comparison data available.</p>'}
                            </div>
                            <div class="tab-pane fade" id="regressions" role="tabpanel">
                                {components[2] if len(components) > 2 else '<p>No regression data available.</p>'}
                            </div>
                        </div>
                    </div>
                </div>
                
                <!-- Additional Information -->
                <div class="row">
                    <div class="col-md-6">
                        <div class="card">
                            <div class="card-header">
                                <h6 class="mb-0"><i class="fas fa-info-circle"></i> About This Dashboard</h6>
                            </div>
                            <div class="card-body">
                                <p>This dashboard provides real-time insights into NexusSynth's performance metrics across different platforms and benchmarks.</p>
                                <ul>
                                    <li><strong>Performance Trends:</strong> Track execution time trends over time</li>
                                    <li><strong>Platform Comparison:</strong> Compare performance across different build environments</li>
                                    <li><strong>Regression Detection:</strong> Automatic detection of performance degradations</li>
                                </ul>
                            </div>
                        </div>
                    </div>
                    <div class="col-md-6">
                        <div class="card">
                            <div class="card-header">
                                <h6 class="mb-0"><i class="fas fa-cog"></i> System Information</h6>
                            </div>
                            <div class="card-body">
                                <p><strong>Monitoring Period:</strong> Last 90 days</p>
                                <p><strong>Update Frequency:</strong> Every commit to main branch</p>
                                <p><strong>Regression Thresholds:</strong></p>
                                <ul class="small">
                                    <li>Minor: 5-15% degradation</li>
                                    <li>Moderate: 15-25% degradation</li>
                                    <li>Major: 25-50% degradation</li>
                                    <li>Critical: >50% degradation</li>
                                </ul>
                            </div>
                        </div>
                    </div>
                </div>
            </div>
            
            <div class="footer">
                <div class="container">
                    <p>&copy; 2024 NexusSynth Performance Monitoring System | Generated at {datetime.now().strftime('%Y-%m-%d %H:%M:%S UTC')}</p>
                </div>
            </div>
            
            <script src="https://cdn.jsdelivr.net/npm/bootstrap@5.1.3/dist/js/bootstrap.bundle.min.js"></script>
        </body>
        </html>
        """
        
        dashboard_path = self.output_dir / "index.html"
        with open(dashboard_path, 'w', encoding='utf-8') as f:
            f.write(html_content)
    
    def _generate_static_reports(self, metrics_df: pd.DataFrame, alerts_df: pd.DataFrame):
        """Generate additional static reports."""
        print("📝 Generating static reports...")
        
        # Generate CSV exports
        if not metrics_df.empty:
            metrics_df.to_csv(self.output_dir / "performance_metrics.csv", index=False)
        
        if not alerts_df.empty:
            alerts_df.to_csv(self.output_dir / "regression_alerts.csv", index=False)
        
        # Generate JSON summary
        summary_data = {
            "generated_at": datetime.now().isoformat(),
            "metrics_count": len(metrics_df),
            "alerts_count": len(alerts_df),
            "unique_benchmarks": metrics_df['benchmark_name'].nunique() if not metrics_df.empty else 0,
            "platforms": metrics_df['platform'].unique().tolist() if not metrics_df.empty else [],
            "date_range": {
                "start": metrics_df['timestamp'].min().isoformat() if not metrics_df.empty else None,
                "end": metrics_df['timestamp'].max().isoformat() if not metrics_df.empty else None
            }
        }
        
        with open(self.output_dir / "dashboard_summary.json", 'w') as f:
            json.dump(summary_data, f, indent=2)
    
    def _generate_empty_dashboard(self):
        """Generate dashboard when no data is available."""
        html_content = """
        <!DOCTYPE html>
        <html lang="en">
        <head>
            <meta charset="UTF-8">
            <meta name="viewport" content="width=device-width, initial-scale=1.0">
            <title>NexusSynth Performance Dashboard</title>
            <link href="https://cdn.jsdelivr.net/npm/bootstrap@5.1.3/dist/css/bootstrap.min.css" rel="stylesheet">
        </head>
        <body>
            <div class="container mt-5">
                <div class="alert alert-warning text-center">
                    <h4>No Performance Data Available</h4>
                    <p>The performance database is empty or no data is available for the selected time period.</p>
                    <p>Performance metrics will appear here after running benchmarks and storing results.</p>
                </div>
            </div>
        </body>
        </html>
        """
        
        with open(self.output_dir / "index.html", 'w') as f:
            f.write(html_content)


def main():
    parser = argparse.ArgumentParser(description='Generate NexusSynth performance dashboard')
    parser.add_argument('--database', type=Path, required=True,
                       help='Path to performance database')
    parser.add_argument('--output', type=Path, required=True,
                       help='Output directory for dashboard')
    parser.add_argument('--days-back', type=int, default=90,
                       help='Days of historical data to include')
    parser.add_argument('--include-trends', action='store_true', default=True,
                       help='Include performance trend analysis')
    parser.add_argument('--include-platform-comparison', action='store_true', default=True,
                       help='Include platform comparison charts')
    parser.add_argument('--include-regression-history', action='store_true', default=True,
                       help='Include regression history timeline')
    
    args = parser.parse_args()
    
    if not args.database.exists():
        print(f"❌ Database file does not exist: {args.database}")
        return 1
    
    dashboard = PerformanceDashboard(args.database, args.output)
    dashboard.generate_dashboard(
        days_back=args.days_back,
        include_trends=args.include_trends,
        include_platform_comparison=args.include_platform_comparison,
        include_regression_history=args.include_regression_history
    )
    
    return 0


if __name__ == '__main__':
    exit(main())