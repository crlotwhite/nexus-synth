#!/usr/bin/env python3
"""
Performance Regression Detection System for NexusSynth

This script implements a comprehensive performance regression detection system that:
1. Maintains a historical performance database
2. Detects performance regressions using statistical analysis
3. Generates alerts and notifications for significant changes
4. Provides trend analysis and dashboard generation
"""

import argparse
import json
import os
import sys
import sqlite3
import statistics
from pathlib import Path
from typing import Dict, List, Optional, Tuple, Any
from datetime import datetime, timedelta
from dataclasses import dataclass, asdict
from enum import Enum

try:
    import pandas as pd
    import numpy as np
    from scipy import stats
    import matplotlib.pyplot as plt
    import seaborn as sns
except ImportError as e:
    print(f"❌ Missing required dependency: {e}")
    print("💡 Install with: pip install pandas matplotlib seaborn numpy scipy")
    sys.exit(1)


class RegressionSeverity(Enum):
    """Severity levels for performance regressions."""
    NONE = "none"
    MINOR = "minor"          # 5-15% degradation
    MODERATE = "moderate"    # 15-25% degradation  
    MAJOR = "major"          # 25-50% degradation
    CRITICAL = "critical"    # >50% degradation


@dataclass
class PerformanceMetric:
    """Represents a single performance measurement."""
    benchmark_name: str
    platform: str
    execution_time_ns: float
    memory_usage: float
    quality_score: float
    timestamp: str
    commit_hash: str
    branch: str
    pr_number: Optional[int] = None
    environment_info: Optional[Dict[str, Any]] = None


@dataclass
class RegressionAlert:
    """Represents a performance regression alert."""
    benchmark_name: str
    platform: str
    severity: RegressionSeverity
    current_value: float
    baseline_value: float
    change_percent: float
    timestamp: str
    commit_hash: str
    statistical_significance: float
    recommendation: str


class PerformanceDatabase:
    """Manages the persistent storage of performance data."""
    
    def __init__(self, db_path: Path):
        self.db_path = db_path
        self.db_path.parent.mkdir(parents=True, exist_ok=True)
        self._init_database()
    
    def _init_database(self):
        """Initialize the SQLite database schema."""
        with sqlite3.connect(self.db_path) as conn:
            conn.execute("""
                CREATE TABLE IF NOT EXISTS performance_metrics (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    benchmark_name TEXT NOT NULL,
                    platform TEXT NOT NULL,
                    execution_time_ns REAL NOT NULL,
                    memory_usage REAL NOT NULL,
                    quality_score REAL NOT NULL,
                    timestamp TEXT NOT NULL,
                    commit_hash TEXT NOT NULL,
                    branch TEXT NOT NULL,
                    pr_number INTEGER,
                    environment_info TEXT,
                    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
                )
            """)
            
            conn.execute("""
                CREATE TABLE IF NOT EXISTS regression_alerts (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    benchmark_name TEXT NOT NULL,
                    platform TEXT NOT NULL,
                    severity TEXT NOT NULL,
                    current_value REAL NOT NULL,
                    baseline_value REAL NOT NULL,
                    change_percent REAL NOT NULL,
                    timestamp TEXT NOT NULL,
                    commit_hash TEXT NOT NULL,
                    statistical_significance REAL NOT NULL,
                    recommendation TEXT NOT NULL,
                    resolved BOOLEAN DEFAULT FALSE,
                    created_at DATETIME DEFAULT CURRENT_TIMESTAMP
                )
            """)
            
            conn.execute("""
                CREATE INDEX IF NOT EXISTS idx_metrics_benchmark_platform 
                ON performance_metrics(benchmark_name, platform, timestamp)
            """)
            
            conn.execute("""
                CREATE INDEX IF NOT EXISTS idx_alerts_severity 
                ON regression_alerts(severity, resolved, created_at)
            """)
    
    def store_metrics(self, metrics: List[PerformanceMetric]):
        """Store performance metrics in the database."""
        with sqlite3.connect(self.db_path) as conn:
            for metric in metrics:
                env_info = json.dumps(metric.environment_info) if metric.environment_info else None
                conn.execute("""
                    INSERT INTO performance_metrics 
                    (benchmark_name, platform, execution_time_ns, memory_usage, 
                     quality_score, timestamp, commit_hash, branch, pr_number, environment_info)
                    VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
                """, (
                    metric.benchmark_name, metric.platform, metric.execution_time_ns,
                    metric.memory_usage, metric.quality_score, metric.timestamp,
                    metric.commit_hash, metric.branch, metric.pr_number, env_info
                ))
    
    def store_alert(self, alert: RegressionAlert):
        """Store a regression alert in the database."""
        with sqlite3.connect(self.db_path) as conn:
            conn.execute("""
                INSERT INTO regression_alerts 
                (benchmark_name, platform, severity, current_value, baseline_value,
                 change_percent, timestamp, commit_hash, statistical_significance, recommendation)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
            """, (
                alert.benchmark_name, alert.platform, alert.severity.value,
                alert.current_value, alert.baseline_value, alert.change_percent,
                alert.timestamp, alert.commit_hash, alert.statistical_significance,
                alert.recommendation
            ))
    
    def get_baseline_metrics(self, benchmark_name: str, platform: str, 
                           days_back: int = 30) -> List[PerformanceMetric]:
        """Get baseline performance metrics for comparison."""
        cutoff_date = (datetime.now() - timedelta(days=days_back)).isoformat()
        
        with sqlite3.connect(self.db_path) as conn:
            cursor = conn.execute("""
                SELECT benchmark_name, platform, execution_time_ns, memory_usage,
                       quality_score, timestamp, commit_hash, branch, pr_number, environment_info
                FROM performance_metrics
                WHERE benchmark_name = ? AND platform = ? 
                  AND timestamp > ? AND branch = 'main'
                ORDER BY timestamp DESC
            """, (benchmark_name, platform, cutoff_date))
            
            metrics = []
            for row in cursor.fetchall():
                env_info = json.loads(row[9]) if row[9] else None
                metrics.append(PerformanceMetric(
                    benchmark_name=row[0], platform=row[1], execution_time_ns=row[2],
                    memory_usage=row[3], quality_score=row[4], timestamp=row[5],
                    commit_hash=row[6], branch=row[7], pr_number=row[8],
                    environment_info=env_info
                ))
            
            return metrics
    
    def get_recent_alerts(self, days_back: int = 7, unresolved_only: bool = True) -> List[RegressionAlert]:
        """Get recent regression alerts."""
        cutoff_date = (datetime.now() - timedelta(days=days_back)).isoformat()
        
        where_clause = "WHERE created_at > ?"
        params = [cutoff_date]
        
        if unresolved_only:
            where_clause += " AND resolved = FALSE"
        
        with sqlite3.connect(self.db_path) as conn:
            cursor = conn.execute(f"""
                SELECT benchmark_name, platform, severity, current_value, baseline_value,
                       change_percent, timestamp, commit_hash, statistical_significance, recommendation
                FROM regression_alerts
                {where_clause}
                ORDER BY created_at DESC
            """, params)
            
            alerts = []
            for row in cursor.fetchall():
                alerts.append(RegressionAlert(
                    benchmark_name=row[0], platform=row[1], severity=RegressionSeverity(row[2]),
                    current_value=row[3], baseline_value=row[4], change_percent=row[5],
                    timestamp=row[6], commit_hash=row[7], statistical_significance=row[8],
                    recommendation=row[9]
                ))
            
            return alerts


class RegressionDetector:
    """Analyzes performance data and detects regressions."""
    
    def __init__(self, database: PerformanceDatabase):
        self.db = database
        
        # Configurable thresholds
        self.thresholds = {
            RegressionSeverity.MINOR: 5.0,      # 5% degradation
            RegressionSeverity.MODERATE: 15.0,  # 15% degradation
            RegressionSeverity.MAJOR: 25.0,     # 25% degradation
            RegressionSeverity.CRITICAL: 50.0   # 50% degradation
        }
        
        # Statistical significance threshold (p-value)
        self.significance_threshold = 0.05
        
        # Minimum sample size for statistical analysis
        self.min_sample_size = 5
    
    def analyze_current_metrics(self, current_metrics: List[PerformanceMetric], 
                              commit_hash: str) -> List[RegressionAlert]:
        """Analyze current metrics against historical baselines."""
        alerts = []
        
        for metric in current_metrics:
            alert = self._analyze_single_metric(metric, commit_hash)
            if alert and alert.severity != RegressionSeverity.NONE:
                alerts.append(alert)
                self.db.store_alert(alert)
        
        return alerts
    
    def _analyze_single_metric(self, current_metric: PerformanceMetric, 
                             commit_hash: str) -> Optional[RegressionAlert]:
        """Analyze a single metric against its baseline."""
        baseline_metrics = self.db.get_baseline_metrics(
            current_metric.benchmark_name, 
            current_metric.platform
        )
        
        if len(baseline_metrics) < self.min_sample_size:
            return None  # Insufficient data for analysis
        
        # Extract execution times for statistical analysis
        baseline_times = [m.execution_time_ns for m in baseline_metrics]
        current_time = current_metric.execution_time_ns
        
        # Calculate statistical measures
        baseline_mean = statistics.mean(baseline_times)
        baseline_std = statistics.stdev(baseline_times) if len(baseline_times) > 1 else 0
        
        # Calculate change percentage
        change_percent = ((current_time - baseline_mean) / baseline_mean) * 100
        
        # Determine severity
        severity = self._calculate_severity(change_percent)
        
        if severity == RegressionSeverity.NONE:
            return None
        
        # Perform statistical significance test (one-sample t-test)
        if baseline_std > 0:
            t_stat, p_value = stats.ttest_1samp(baseline_times, current_time)
            statistical_significance = p_value
        else:
            statistical_significance = 1.0  # No variation in baseline
        
        # Generate recommendation
        recommendation = self._generate_recommendation(severity, change_percent, 
                                                     current_metric.benchmark_name)
        
        return RegressionAlert(
            benchmark_name=current_metric.benchmark_name,
            platform=current_metric.platform,
            severity=severity,
            current_value=current_time,
            baseline_value=baseline_mean,
            change_percent=change_percent,
            timestamp=current_metric.timestamp,
            commit_hash=commit_hash,
            statistical_significance=statistical_significance,
            recommendation=recommendation
        )
    
    def _calculate_severity(self, change_percent: float) -> RegressionSeverity:
        """Calculate regression severity based on change percentage."""
        abs_change = abs(change_percent)
        
        # Only consider performance degradations (positive changes) as regressions
        if change_percent <= 0:  # Performance improvement
            return RegressionSeverity.NONE
        
        if abs_change >= self.thresholds[RegressionSeverity.CRITICAL]:
            return RegressionSeverity.CRITICAL
        elif abs_change >= self.thresholds[RegressionSeverity.MAJOR]:
            return RegressionSeverity.MAJOR
        elif abs_change >= self.thresholds[RegressionSeverity.MODERATE]:
            return RegressionSeverity.MODERATE
        elif abs_change >= self.thresholds[RegressionSeverity.MINOR]:
            return RegressionSeverity.MINOR
        else:
            return RegressionSeverity.NONE
    
    def _generate_recommendation(self, severity: RegressionSeverity, 
                               change_percent: float, benchmark_name: str) -> str:
        """Generate actionable recommendations based on regression analysis."""
        base_recommendations = {
            RegressionSeverity.MINOR: [
                "Monitor this benchmark in upcoming commits",
                "Consider profiling if regression persists",
                "Review recent algorithmic changes"
            ],
            RegressionSeverity.MODERATE: [
                "Investigate recent code changes affecting this benchmark",
                "Run detailed profiling to identify bottlenecks", 
                "Consider reverting recent performance-affecting commits"
            ],
            RegressionSeverity.MAJOR: [
                "Immediate investigation required",
                "Profile the specific code paths in this benchmark",
                "Consider blocking deployment until resolved"
            ],
            RegressionSeverity.CRITICAL: [
                "Critical performance degradation detected",
                "Block deployment immediately",
                "Emergency profiling and optimization required"
            ]
        }
        
        recommendations = base_recommendations.get(severity, ["No specific recommendation"])
        
        # Add benchmark-specific recommendations
        if "synthesis" in benchmark_name.lower():
            recommendations.append("Check WORLD vocoder parameter settings")
            recommendations.append("Verify HMM model loading performance")
        elif "training" in benchmark_name.lower():
            recommendations.append("Review training data preprocessing pipeline")
            recommendations.append("Check memory allocation patterns in ML operations")
        
        return " • ".join(recommendations)


class NotificationSystem:
    """Handles alerts and notifications for performance regressions."""
    
    def __init__(self, webhook_urls: Optional[Dict[str, str]] = None):
        self.webhook_urls = webhook_urls or {}
    
    def send_github_issue(self, alerts: List[RegressionAlert], repo_info: Dict[str, str]):
        """Create GitHub issues for critical performance regressions."""
        critical_alerts = [a for a in alerts if a.severity == RegressionSeverity.CRITICAL]
        major_alerts = [a for a in alerts if a.severity == RegressionSeverity.MAJOR]
        
        if not critical_alerts and not major_alerts:
            return
        
        # Generate issue content
        title = f"🚨 Performance Regression Detected ({len(critical_alerts + major_alerts)} benchmarks)"
        
        body = "## 🚨 Performance Regression Alert\n\n"
        body += f"**Detection Time:** {datetime.now().strftime('%Y-%m-%d %H:%M:%S UTC')}\n"
        body += f"**Commit:** {alerts[0].commit_hash if alerts else 'Unknown'}\n\n"
        
        if critical_alerts:
            body += "### 🔴 Critical Regressions (>50% degradation)\n\n"
            body += "| Benchmark | Platform | Change | Recommendation |\n"
            body += "|-----------|----------|--------|----------------|\n"
            
            for alert in critical_alerts:
                body += f"| {alert.benchmark_name} | {alert.platform} | "
                body += f"+{alert.change_percent:.1f}% | {alert.recommendation[:100]}... |\n"
        
        if major_alerts:
            body += "### 🟡 Major Regressions (25-50% degradation)\n\n"
            body += "| Benchmark | Platform | Change | Recommendation |\n"
            body += "|-----------|----------|--------|----------------|\n"
            
            for alert in major_alerts:
                body += f"| {alert.benchmark_name} | {alert.platform} | "
                body += f"+{alert.change_percent:.1f}% | {alert.recommendation[:100]}... |\n"
        
        body += "\n## 📊 Next Steps\n\n"
        body += "1. **Immediate:** Review the commits leading to this regression\n"
        body += "2. **Profile:** Run detailed performance profiling on affected benchmarks\n"
        body += "3. **Investigate:** Check for algorithmic changes or dependency updates\n"
        body += "4. **Validate:** Confirm the regression with manual testing\n"
        body += "5. **Fix:** Apply optimizations or revert problematic changes\n\n"
        body += "*This issue was automatically generated by the NexusSynth Performance Regression Detection System.*"
        
        # Output GitHub issue creation script
        issue_script = f"""
# GitHub Issue Creation Script
# Run this to create a GitHub issue for the performance regression

gh issue create \\
  --title "{title}" \\
  --body "{body}" \\
  --label "performance,regression,high-priority" \\
  --assignee "performance-team"
"""
        
        with open("performance_regression_issue.sh", "w") as f:
            f.write(issue_script)
        
        print("📝 GitHub issue creation script saved to: performance_regression_issue.sh")
    
    def generate_summary_report(self, alerts: List[RegressionAlert], 
                              output_path: Path) -> None:
        """Generate a comprehensive summary report."""
        output_path.mkdir(parents=True, exist_ok=True)
        
        # Generate markdown report
        report_path = output_path / "regression_summary.md"
        with open(report_path, 'w') as f:
            f.write("# 🚨 Performance Regression Detection Report\n\n")
            f.write(f"**Generated:** {datetime.now().strftime('%Y-%m-%d %H:%M:%S UTC')}\n\n")
            
            if not alerts:
                f.write("✅ **No performance regressions detected.**\n")
                return
            
            # Summary statistics
            severity_counts = {}
            for severity in RegressionSeverity:
                count = len([a for a in alerts if a.severity == severity])
                if count > 0:
                    severity_counts[severity] = count
            
            f.write("## 📊 Summary\n\n")
            f.write(f"**Total Regressions:** {len(alerts)}\n\n")
            
            for severity, count in severity_counts.items():
                emoji = {"critical": "🔴", "major": "🟡", "moderate": "🟠", "minor": "🟢"}.get(severity.value, "⚪")
                f.write(f"- {emoji} **{severity.value.title()}:** {count}\n")
            
            f.write("\n## 🔍 Detailed Analysis\n\n")
            
            # Group alerts by severity
            for severity in [RegressionSeverity.CRITICAL, RegressionSeverity.MAJOR, 
                           RegressionSeverity.MODERATE, RegressionSeverity.MINOR]:
                severity_alerts = [a for a in alerts if a.severity == severity]
                if not severity_alerts:
                    continue
                
                emoji = {"critical": "🔴", "major": "🟡", "moderate": "🟠", "minor": "🟢"}.get(severity.value, "⚪")
                f.write(f"### {emoji} {severity.value.title()} Regressions\n\n")
                f.write("| Benchmark | Platform | Change | Baseline | Current | Recommendation |\n")
                f.write("|-----------|----------|--------|----------|---------|----------------|\n")
                
                for alert in severity_alerts:
                    baseline_ms = alert.baseline_value / 1_000_000
                    current_ms = alert.current_value / 1_000_000
                    f.write(f"| {alert.benchmark_name} | {alert.platform} | "
                           f"+{alert.change_percent:.1f}% | {baseline_ms:.2f}ms | "
                           f"{current_ms:.2f}ms | {alert.recommendation[:80]}... |\n")
        
        print(f"📄 Regression summary report saved to: {report_path}")


def load_benchmark_results(input_dir: Path) -> List[PerformanceMetric]:
    """Load benchmark results from JSON files and convert to PerformanceMetric objects."""
    metrics = []
    
    for json_file in input_dir.rglob("*.json"):
        try:
            with open(json_file, 'r') as f:
                data = json.load(f)
            
            # Extract commit hash and other metadata
            commit_hash = os.environ.get('GITHUB_SHA', 'unknown')
            branch = os.environ.get('GITHUB_REF_NAME', 'main')
            pr_number = None
            
            if 'pull_request' in os.environ.get('GITHUB_EVENT_NAME', ''):
                pr_number = os.environ.get('GITHUB_EVENT_NUMBER')
            
            # Process benchmark data
            if isinstance(data, list):
                benchmark_list = data
            elif isinstance(data, dict) and 'results' in data:
                benchmark_list = data['results']
            else:
                benchmark_list = [data]
            
            for benchmark in benchmark_list:
                if not benchmark.get('benchmark_successful', True):
                    continue
                
                # Extract platform from file path
                platform = "Unknown Platform"
                path_str = str(json_file)
                if "Windows" in path_str or "windows" in path_str:
                    platform = "Windows x64"
                elif "Ubuntu" in path_str and "GCC" in path_str:
                    platform = "Ubuntu GCC"
                elif "Ubuntu" in path_str and "Clang" in path_str:
                    platform = "Ubuntu Clang"
                elif "macOS" in path_str:
                    platform = "macOS"
                
                metrics.append(PerformanceMetric(
                    benchmark_name=benchmark.get('benchmark_name', 'unknown'),
                    platform=platform,
                    execution_time_ns=float(benchmark.get('avg_execution_time_ns', 0)),
                    memory_usage=float(benchmark.get('avg_memory_usage', 0)),
                    quality_score=float(benchmark.get('formant_preservation_score', 0.0)),
                    timestamp=datetime.now().isoformat(),
                    commit_hash=commit_hash,
                    branch=branch,
                    pr_number=int(pr_number) if pr_number else None,
                    environment_info=benchmark.get('environment_info')
                ))
                
        except (json.JSONDecodeError, KeyError, ValueError) as e:
            print(f"⚠️ Failed to parse {json_file}: {e}")
            continue
    
    return metrics


def main():
    parser = argparse.ArgumentParser(description='NexusSynth Performance Regression Detector')
    parser.add_argument('--input', type=Path, required=True,
                       help='Input directory containing benchmark results')
    parser.add_argument('--database', type=Path, default=Path('.performance_db/metrics.db'),
                       help='Path to performance database')
    parser.add_argument('--output', type=Path, default=Path('./regression_analysis'),
                       help='Output directory for reports and alerts')
    parser.add_argument('--commit-hash', 
                       default=os.environ.get('GITHUB_SHA', 'unknown'),
                       help='Commit hash for this analysis')
    parser.add_argument('--create-github-issues', action='store_true',
                       help='Create GitHub issues for critical regressions')
    parser.add_argument('--baseline-days', type=int, default=30,
                       help='Days of historical data to use for baseline')
    
    args = parser.parse_args()
    
    if not args.input.exists():
        print(f"❌ Input directory does not exist: {args.input}")
        return 1
    
    # Initialize components
    database = PerformanceDatabase(args.database)
    detector = RegressionDetector(database)
    notifier = NotificationSystem()
    
    # Load current metrics
    print(f"📊 Loading benchmark results from {args.input}")
    current_metrics = load_benchmark_results(args.input)
    
    if not current_metrics:
        print("❌ No valid benchmark data found")
        return 1
    
    print(f"✅ Loaded {len(current_metrics)} performance metrics")
    
    # Store current metrics in database
    database.store_metrics(current_metrics)
    print("💾 Stored metrics in performance database")
    
    # Analyze for regressions
    print("🔍 Analyzing for performance regressions...")
    alerts = detector.analyze_current_metrics(current_metrics, args.commit_hash)
    
    if alerts:
        print(f"⚠️ Detected {len(alerts)} performance regressions")
        
        # Generate reports
        notifier.generate_summary_report(alerts, args.output)
        
        # Create GitHub issues if requested
        if args.create_github_issues:
            repo_info = {
                'owner': os.environ.get('GITHUB_REPOSITORY_OWNER', ''),
                'repo': os.environ.get('GITHUB_REPOSITORY', '').split('/')[-1] if '/' in os.environ.get('GITHUB_REPOSITORY', '') else ''
            }
            notifier.send_github_issue(alerts, repo_info)
    else:
        print("✅ No performance regressions detected")
    
    # Output summary for GitHub Actions
    if 'GITHUB_OUTPUT' in os.environ:
        with open(os.environ['GITHUB_OUTPUT'], 'a') as f:
            f.write(f"regression-count={len(alerts)}\n")
            f.write(f"critical-regressions={len([a for a in alerts if a.severity == RegressionSeverity.CRITICAL])}\n")
            f.write(f"has-regressions={'true' if alerts else 'false'}\n")
    
    return 0


if __name__ == '__main__':
    sys.exit(main())