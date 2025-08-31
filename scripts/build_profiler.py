#!/usr/bin/env python3
"""
Build Profiler for uSEQ Project
Analyzes build performance and generates detailed reports
"""

import argparse
import json
import os
import subprocess
import sys
import time
from dataclasses import dataclass, asdict
from datetime import datetime
from pathlib import Path
from typing import List, Dict, Any, Optional
import statistics

# Try to import optional dependencies
try:
    import matplotlib.pyplot as plt
    MATPLOTLIB_AVAILABLE = True
except ImportError:
    MATPLOTLIB_AVAILABLE = False
    print("Warning: matplotlib not available, charts will be disabled", file=sys.stderr)

try:
    import pandas as pd
    PANDAS_AVAILABLE = True
except ImportError:
    PANDAS_AVAILABLE = False


@dataclass
class BuildResult:
    """Represents a single build measurement"""
    timestamp: str
    config: str
    build_type: str  # clean or incremental
    unity_build: bool
    ccache_enabled: bool
    total_time: float
    config_time: Optional[float] = None
    compile_time: Optional[float] = None
    link_time: Optional[float] = None
    files_compiled: Optional[int] = None
    
    def to_dict(self) -> Dict[str, Any]:
        return asdict(self)


class BuildProfiler:
    """Main build profiler class"""
    
    def __init__(self, project_root: Path):
        self.project_root = project_root
        self.results: List[BuildResult] = []
        self.build_dir = project_root / "build"
        
    def run_build_measurement(self, config: str = "release", 
                            unity: bool = False, 
                            ccache: bool = True,
                            incremental: bool = False) -> BuildResult:
        """Run a single build measurement"""
        
        print(f"Measuring {'incremental' if incremental else 'clean'} build: "
              f"config={config}, unity={unity}, ccache={ccache}")
        
        if not incremental:
            # Clean build
            if self.build_dir.exists():
                subprocess.run(["rm", "-rf", str(self.build_dir)], check=True)
            
            # Configure
            meson_args = ["meson", "setup", str(self.build_dir), f"--buildtype={config}"]
            if unity:
                meson_args.append("-Dunity_build=true")
            if ccache:
                meson_args.append("-Duse_ccache=true")
            else:
                meson_args.append("-Duse_ccache=false")
            
            config_start = time.time()
            subprocess.run(meson_args, check=True, capture_output=True)
            config_time = time.time() - config_start
            
            # Build
            build_start = time.time()
            try:
                result = subprocess.run(
                    ["ninja", "-C", str(self.build_dir), "-v"],
                    check=True, 
                    capture_output=True, 
                    text=True
                )
            except subprocess.CalledProcessError as e:
                # Try without -v if verbose fails
                result = subprocess.run(
                    ["ninja", "-C", str(self.build_dir)],
                    check=True, 
                    capture_output=True, 
                    text=True
                )
            build_time = time.time() - build_start
            
            # Count compiled files
            files_compiled = result.stdout.count("Compiling")
            
            return BuildResult(
                timestamp=datetime.now().isoformat(),
                config=config,
                build_type="clean",
                unity_build=unity,
                ccache_enabled=ccache,
                total_time=config_time + build_time,
                config_time=config_time,
                compile_time=build_time,
                files_compiled=files_compiled
            )
        else:
            # Incremental build - touch a file first
            test_file = self.project_root / "uSEQ/src/modulisp/lisp/value.cpp"
            if test_file.exists():
                test_file.touch()
            
            build_start = time.time()
            subprocess.run(
                ["ninja", "-C", str(self.build_dir)],
                check=True,
                capture_output=True
            )
            build_time = time.time() - build_start
            
            return BuildResult(
                timestamp=datetime.now().isoformat(),
                config=config,
                build_type="incremental",
                unity_build=unity,
                ccache_enabled=ccache,
                total_time=build_time
            )
    
    def analyze_ninja_log(self) -> Dict[str, Any]:
        """Analyze ninja build log for detailed timing"""
        
        ninja_log = self.build_dir / ".ninja_log"
        if not ninja_log.exists():
            return {}
        
        file_times = {}
        with open(ninja_log, 'r') as f:
            for line in f:
                if line.startswith("#"):
                    continue
                parts = line.strip().split('\t')
                if len(parts) >= 5:
                    start_time = int(parts[0])
                    end_time = int(parts[1])
                    output_file = parts[3]
                    duration = (end_time - start_time) / 1000.0  # Convert to seconds
                    
                    # Extract filename from path
                    if output_file.endswith('.o'):
                        file_times[output_file] = duration
        
        if file_times:
            # Calculate statistics
            times = list(file_times.values())
            return {
                "total_files": len(file_times),
                "total_time": sum(times),
                "average_time": statistics.mean(times),
                "median_time": statistics.median(times),
                "max_time": max(times),
                "min_time": min(times),
                "slowest_files": sorted(file_times.items(), key=lambda x: x[1], reverse=True)[:10]
            }
        
        return {}
    
    def run_full_profile(self) -> None:
        """Run a complete build profile with multiple configurations"""
        
        configurations = [
            ("debug", False, False),
            ("debug", False, True),
            ("debug", True, True),
            ("release", False, False),
            ("release", False, True),
            ("release", True, True),
            ("minsize", False, True),
        ]
        
        for config, unity, ccache in configurations:
            try:
                # Clean build
                result = self.run_build_measurement(config, unity, ccache, incremental=False)
                self.results.append(result)
                
                # Incremental build
                result = self.run_build_measurement(config, unity, ccache, incremental=True)
                self.results.append(result)
                
            except subprocess.CalledProcessError as e:
                print(f"Build failed: {e}", file=sys.stderr)
                continue
        
        # Analyze ninja log for the last build
        ninja_stats = self.analyze_ninja_log()
        if ninja_stats:
            print("\n=== Ninja Build Log Analysis ===")
            print(f"Total files compiled: {ninja_stats['total_files']}")
            print(f"Average compile time: {ninja_stats['average_time']:.2f}s")
            print(f"Median compile time: {ninja_stats['median_time']:.2f}s")
            print(f"Slowest files:")
            for file, time_taken in ninja_stats['slowest_files']:
                print(f"  {Path(file).name}: {time_taken:.2f}s")
    
    def generate_report(self, output_file: Optional[Path] = None) -> None:
        """Generate a detailed report of build measurements"""
        
        if not self.results:
            print("No results to report", file=sys.stderr)
            return
        
        # Group results
        clean_builds = [r for r in self.results if r.build_type == "clean"]
        incremental_builds = [r for r in self.results if r.build_type == "incremental"]
        
        report = {
            "timestamp": datetime.now().isoformat(),
            "project_root": str(self.project_root),
            "total_measurements": len(self.results),
            "clean_builds": [r.to_dict() for r in clean_builds],
            "incremental_builds": [r.to_dict() for r in incremental_builds],
            "summary": {
                "fastest_clean": min((r.total_time for r in clean_builds), default=0),
                "slowest_clean": max((r.total_time for r in clean_builds), default=0),
                "average_clean": statistics.mean([r.total_time for r in clean_builds]) if clean_builds else 0,
                "fastest_incremental": min((r.total_time for r in incremental_builds), default=0),
                "slowest_incremental": max((r.total_time for r in incremental_builds), default=0),
                "average_incremental": statistics.mean([r.total_time for r in incremental_builds]) if incremental_builds else 0,
            }
        }
        
        # Print summary
        print("\n=== Build Profile Summary ===")
        print(f"Total measurements: {len(self.results)}")
        print(f"\nClean builds:")
        print(f"  Fastest: {report['summary']['fastest_clean']:.2f}s")
        print(f"  Slowest: {report['summary']['slowest_clean']:.2f}s")
        print(f"  Average: {report['summary']['average_clean']:.2f}s")
        print(f"\nIncremental builds:")
        print(f"  Fastest: {report['summary']['fastest_incremental']:.2f}s")
        print(f"  Slowest: {report['summary']['slowest_incremental']:.2f}s")
        print(f"  Average: {report['summary']['average_incremental']:.2f}s")
        
        # Best configurations
        if clean_builds:
            best_clean = min(clean_builds, key=lambda r: r.total_time)
            print(f"\nBest clean build configuration:")
            print(f"  Config: {best_clean.config}")
            print(f"  Unity: {best_clean.unity_build}")
            print(f"  Ccache: {best_clean.ccache_enabled}")
            print(f"  Time: {best_clean.total_time:.2f}s")
        
        if incremental_builds:
            best_incremental = min(incremental_builds, key=lambda r: r.total_time)
            print(f"\nBest incremental build configuration:")
            print(f"  Config: {best_incremental.config}")
            print(f"  Unity: {best_incremental.unity_build}")
            print(f"  Ccache: {best_incremental.ccache_enabled}")
            print(f"  Time: {best_incremental.total_time:.2f}s")
        
        # Save to file if requested
        if output_file:
            with open(output_file, 'w') as f:
                json.dump(report, f, indent=2)
            print(f"\nReport saved to: {output_file}")
    
    def generate_charts(self, output_dir: Path) -> None:
        """Generate visualization charts if matplotlib is available"""
        
        if not MATPLOTLIB_AVAILABLE:
            print("matplotlib not available, skipping charts", file=sys.stderr)
            return
        
        if not self.results:
            print("No results to chart", file=sys.stderr)
            return
        
        output_dir.mkdir(parents=True, exist_ok=True)
        
        # Prepare data
        clean_builds = [r for r in self.results if r.build_type == "clean"]
        incremental_builds = [r for r in self.results if r.build_type == "incremental"]
        
        # Chart 1: Clean vs Incremental comparison
        fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(12, 5))
        
        # Clean builds
        if clean_builds:
            labels = [f"{r.config}\nunity={r.unity_build}\nccache={r.ccache_enabled}" 
                     for r in clean_builds]
            times = [r.total_time for r in clean_builds]
            ax1.bar(range(len(times)), times)
            ax1.set_xticks(range(len(times)))
            ax1.set_xticklabels(labels, rotation=45, ha='right')
            ax1.set_ylabel('Time (seconds)')
            ax1.set_title('Clean Build Times')
            ax1.grid(True, alpha=0.3)
        
        # Incremental builds
        if incremental_builds:
            labels = [f"{r.config}\nunity={r.unity_build}\nccache={r.ccache_enabled}" 
                     for r in incremental_builds]
            times = [r.total_time for r in incremental_builds]
            ax2.bar(range(len(times)), times)
            ax2.set_xticks(range(len(times)))
            ax2.set_xticklabels(labels, rotation=45, ha='right')
            ax2.set_ylabel('Time (seconds)')
            ax2.set_title('Incremental Build Times')
            ax2.grid(True, alpha=0.3)
        
        plt.tight_layout()
        plt.savefig(output_dir / 'build_times_comparison.png', dpi=150)
        plt.close()
        
        # Chart 2: Impact of optimizations
        fig, ax = plt.subplots(figsize=(10, 6))
        
        # Group by optimization
        optimizations = {
            'baseline': [],
            'ccache': [],
            'unity': [],
            'both': []
        }
        
        for r in clean_builds:
            if not r.unity_build and not r.ccache_enabled:
                optimizations['baseline'].append(r.total_time)
            elif not r.unity_build and r.ccache_enabled:
                optimizations['ccache'].append(r.total_time)
            elif r.unity_build and not r.ccache_enabled:
                optimizations['unity'].append(r.total_time)
            elif r.unity_build and r.ccache_enabled:
                optimizations['both'].append(r.total_time)
        
        # Calculate averages
        opt_names = []
        opt_times = []
        for name, times in optimizations.items():
            if times:
                opt_names.append(name)
                opt_times.append(statistics.mean(times))
        
        if opt_names:
            ax.bar(opt_names, opt_times)
            ax.set_ylabel('Average Time (seconds)')
            ax.set_title('Impact of Build Optimizations')
            ax.grid(True, alpha=0.3)
            
            # Add percentage improvements
            if 'baseline' in opt_names and optimizations['baseline']:
                baseline_time = statistics.mean(optimizations['baseline'])
                for i, (name, time) in enumerate(zip(opt_names, opt_times)):
                    if name != 'baseline':
                        improvement = ((baseline_time - time) / baseline_time) * 100
                        ax.text(i, time, f'-{improvement:.1f}%', ha='center', va='bottom')
        
        plt.tight_layout()
        plt.savefig(output_dir / 'optimization_impact.png', dpi=150)
        plt.close()
        
        print(f"Charts saved to: {output_dir}")


def main():
    """Main entry point"""
    
    parser = argparse.ArgumentParser(
        description="Build profiler for uSEQ project",
        formatter_class=argparse.RawDescriptionHelpFormatter
    )
    
    parser.add_argument(
        '-p', '--project-root',
        type=Path,
        default=Path.cwd(),
        help='Project root directory (default: current directory)'
    )
    
    parser.add_argument(
        '-o', '--output',
        type=Path,
        help='Output file for JSON report'
    )
    
    parser.add_argument(
        '-c', '--charts',
        type=Path,
        help='Directory for chart output'
    )
    
    parser.add_argument(
        '--quick',
        action='store_true',
        help='Quick test with single configuration'
    )
    
    parser.add_argument(
        '--analyze-only',
        action='store_true',
        help='Only analyze existing ninja log'
    )
    
    args = parser.parse_args()
    
    # Create profiler
    profiler = BuildProfiler(args.project_root)
    
    if args.analyze_only:
        # Just analyze existing build
        stats = profiler.analyze_ninja_log()
        if stats:
            print(json.dumps(stats, indent=2))
        else:
            print("No ninja log found", file=sys.stderr)
            sys.exit(1)
    elif args.quick:
        # Quick single measurement
        result = profiler.run_build_measurement(
            config="release",
            unity=False,
            ccache=True,
            incremental=False
        )
        print(f"Build time: {result.total_time:.2f}s")
        profiler.results.append(result)
    else:
        # Full profile
        profiler.run_full_profile()
    
    # Generate report
    if profiler.results:
        profiler.generate_report(args.output)
        
        if args.charts:
            profiler.generate_charts(args.charts)


if __name__ == "__main__":
    main()