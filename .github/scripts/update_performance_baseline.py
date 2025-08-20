#!/usr/bin/env python3
"""
Performance Baseline Management Script for NexusSynth

This script manages the performance baseline database by:
1. Cleaning old performance data
2. Updating baseline metrics
3. Optimizing database performance
4. Managing database size
"""

import argparse
import sqlite3
import os
from pathlib import Path
from datetime import datetime, timedelta


class BaselineManager:
    """Manages performance baseline data and database maintenance."""
    
    def __init__(self, db_path: Path):
        self.db_path = db_path
        if not self.db_path.exists():
            raise FileNotFoundError(f"Database not found: {db_path}")
    
    def update_baseline(self, commit_hash: str, branch: str):
        """Update baseline performance metrics for the specified commit."""
        print(f"📊 Updating performance baseline for commit {commit_hash[:8]} on {branch}")
        
        with sqlite3.connect(self.db_path) as conn:
            # Get current metrics count
            cursor = conn.execute("SELECT COUNT(*) FROM performance_metrics")
            total_metrics = cursor.fetchone()[0]
            
            # Get metrics for current commit
            cursor = conn.execute("""
                SELECT COUNT(*) FROM performance_metrics 
                WHERE commit_hash = ? AND branch = ?
            """, (commit_hash, branch))
            current_metrics = cursor.fetchone()[0]
            
            print(f"✅ Database contains {total_metrics} total metrics")
            print(f"✅ Current commit has {current_metrics} metrics")
    
    def cleanup_old_data(self, retain_days: int = 365):
        """Remove old performance data beyond retention period."""
        cutoff_date = (datetime.now() - timedelta(days=retain_days)).isoformat()
        
        print(f"🧹 Cleaning up data older than {retain_days} days...")
        
        with sqlite3.connect(self.db_path) as conn:
            # Count records to be deleted
            cursor = conn.execute("""
                SELECT COUNT(*) FROM performance_metrics WHERE timestamp < ?
            """, (cutoff_date,))
            old_metrics_count = cursor.fetchone()[0]
            
            cursor = conn.execute("""
                SELECT COUNT(*) FROM regression_alerts WHERE created_at < ?
            """, (cutoff_date,))
            old_alerts_count = cursor.fetchone()[0]
            
            if old_metrics_count > 0 or old_alerts_count > 0:
                # Delete old metrics
                conn.execute("""
                    DELETE FROM performance_metrics WHERE timestamp < ?
                """, (cutoff_date,))
                
                # Delete old alerts
                conn.execute("""
                    DELETE FROM regression_alerts WHERE created_at < ?
                """, (cutoff_date,))
                
                print(f"🗑️ Removed {old_metrics_count} old metrics and {old_alerts_count} old alerts")
            else:
                print("✅ No old data to clean up")
    
    def optimize_database(self):
        """Optimize database performance and reclaim space."""
        print("⚡ Optimizing database...")
        
        with sqlite3.connect(self.db_path) as conn:
            # Analyze tables for better query planning
            conn.execute("ANALYZE")
            
            # Vacuum to reclaim space and defragment
            conn.execute("VACUUM")
            
            # Update statistics
            conn.execute("PRAGMA optimize")
            
        print("✅ Database optimization completed")
    
    def get_database_stats(self):
        """Get database statistics and health information."""
        print("📊 Database Statistics:")
        
        with sqlite3.connect(self.db_path) as conn:
            # Table sizes
            cursor = conn.execute("SELECT COUNT(*) FROM performance_metrics")
            metrics_count = cursor.fetchone()[0]
            
            cursor = conn.execute("SELECT COUNT(*) FROM regression_alerts")
            alerts_count = cursor.fetchone()[0]
            
            # Date range
            cursor = conn.execute("""
                SELECT MIN(timestamp), MAX(timestamp) FROM performance_metrics
            """)
            date_range = cursor.fetchone()
            
            # Platform and benchmark diversity
            cursor = conn.execute("SELECT COUNT(DISTINCT platform) FROM performance_metrics")
            unique_platforms = cursor.fetchone()[0]
            
            cursor = conn.execute("SELECT COUNT(DISTINCT benchmark_name) FROM performance_metrics")
            unique_benchmarks = cursor.fetchone()[0]
            
            # Database file size
            db_size_mb = self.db_path.stat().st_size / (1024 * 1024)
            
            print(f"  📈 Performance Metrics: {metrics_count:,}")
            print(f"  🚨 Regression Alerts: {alerts_count:,}")
            print(f"  🖥️  Unique Platforms: {unique_platforms}")
            print(f"  🧪 Unique Benchmarks: {unique_benchmarks}")
            print(f"  📅 Date Range: {date_range[0]} to {date_range[1]}")
            print(f"  💾 Database Size: {db_size_mb:.2f} MB")
    
    def validate_database_integrity(self):
        """Validate database integrity and consistency."""
        print("🔍 Validating database integrity...")
        
        issues_found = 0
        
        with sqlite3.connect(self.db_path) as conn:
            try:
                # Check for SQLite integrity
                cursor = conn.execute("PRAGMA integrity_check")
                integrity_result = cursor.fetchone()[0]
                
                if integrity_result != "ok":
                    print(f"❌ Database integrity check failed: {integrity_result}")
                    issues_found += 1
                else:
                    print("✅ Database integrity check passed")
                
                # Check for orphaned data
                cursor = conn.execute("""
                    SELECT COUNT(*) FROM performance_metrics 
                    WHERE execution_time_ns <= 0 OR timestamp IS NULL
                """)
                invalid_metrics = cursor.fetchone()[0]
                
                if invalid_metrics > 0:
                    print(f"⚠️ Found {invalid_metrics} metrics with invalid data")
                    issues_found += 1
                
                # Check for duplicate entries
                cursor = conn.execute("""
                    SELECT benchmark_name, platform, timestamp, commit_hash, COUNT(*) 
                    FROM performance_metrics 
                    GROUP BY benchmark_name, platform, timestamp, commit_hash 
                    HAVING COUNT(*) > 1
                """)
                duplicates = cursor.fetchall()
                
                if duplicates:
                    print(f"⚠️ Found {len(duplicates)} sets of duplicate metrics")
                    issues_found += 1
                
            except sqlite3.Error as e:
                print(f"❌ Database validation error: {e}")
                issues_found += 1
        
        if issues_found == 0:
            print("✅ Database validation completed - no issues found")
        else:
            print(f"⚠️ Database validation completed - {issues_found} issues found")
        
        return issues_found == 0
    
    def backup_database(self, backup_dir: Path):
        """Create a backup of the performance database."""
        backup_dir.mkdir(parents=True, exist_ok=True)
        
        timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        backup_path = backup_dir / f"performance_db_backup_{timestamp}.db"
        
        print(f"💾 Creating database backup: {backup_path}")
        
        try:
            with sqlite3.connect(self.db_path) as source:
                with sqlite3.connect(backup_path) as backup:
                    source.backup(backup)
            
            print(f"✅ Backup created successfully: {backup_path}")
            return backup_path
            
        except sqlite3.Error as e:
            print(f"❌ Backup failed: {e}")
            return None
    
    def cleanup_old_backups(self, backup_dir: Path, retain_backups: int = 10):
        """Remove old backup files, keeping only the most recent ones."""
        if not backup_dir.exists():
            return
        
        backup_files = sorted(
            backup_dir.glob("performance_db_backup_*.db"),
            key=lambda x: x.stat().st_mtime,
            reverse=True
        )
        
        if len(backup_files) > retain_backups:
            old_backups = backup_files[retain_backups:]
            for backup_file in old_backups:
                backup_file.unlink()
                print(f"🗑️ Removed old backup: {backup_file.name}")
            
            print(f"✅ Kept {retain_backups} most recent backups")


def main():
    parser = argparse.ArgumentParser(description='Manage NexusSynth performance baseline database')
    parser.add_argument('--database', type=Path, required=True,
                       help='Path to performance database')
    parser.add_argument('--commit-hash', required=True,
                       help='Commit hash for baseline update')
    parser.add_argument('--branch', default='main',
                       help='Branch name for baseline update')
    parser.add_argument('--cleanup-old-data', action='store_true',
                       help='Remove old performance data')
    parser.add_argument('--retain-days', type=int, default=365,
                       help='Days of data to retain during cleanup')
    parser.add_argument('--optimize', action='store_true',
                       help='Optimize database performance')
    parser.add_argument('--validate', action='store_true',
                       help='Validate database integrity')
    parser.add_argument('--backup', action='store_true',
                       help='Create database backup')
    parser.add_argument('--backup-dir', type=Path, default=Path('.database_backups'),
                       help='Directory for database backups')
    parser.add_argument('--stats', action='store_true',
                       help='Show database statistics')
    
    args = parser.parse_args()
    
    if not args.database.exists():
        print(f"❌ Database file does not exist: {args.database}")
        return 1
    
    try:
        manager = BaselineManager(args.database)
        
        # Show initial stats if requested
        if args.stats:
            manager.get_database_stats()
            print()
        
        # Create backup if requested
        if args.backup:
            backup_path = manager.backup_database(args.backup_dir)
            if backup_path:
                manager.cleanup_old_backups(args.backup_dir)
            print()
        
        # Validate database integrity
        if args.validate:
            is_valid = manager.validate_database_integrity()
            if not is_valid:
                print("⚠️ Database validation failed - consider restoring from backup")
                return 1
            print()
        
        # Update baseline metrics
        manager.update_baseline(args.commit_hash, args.branch)
        print()
        
        # Cleanup old data if requested
        if args.cleanup_old_data:
            manager.cleanup_old_data(args.retain_days)
            print()
        
        # Optimize database if requested
        if args.optimize:
            manager.optimize_database()
            print()
        
        # Show final stats
        manager.get_database_stats()
        
        print("🎉 Baseline management completed successfully!")
        return 0
        
    except Exception as e:
        print(f"❌ Baseline management failed: {e}")
        return 1


if __name__ == '__main__':
    exit(main())