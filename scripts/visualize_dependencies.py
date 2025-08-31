#!/usr/bin/env python3
"""
Build Dependency Visualization Tool for uSEQ Project

This tool analyzes the Meson build system to generate dependency graphs showing
which files trigger the most rebuilds when modified. It helps identify build
bottlenecks and optimization opportunities.

Usage:
    ./visualize_dependencies.py                 # Generate all visualizations
    ./visualize_dependencies.py --format svg    # Generate SVG output
    ./visualize_dependencies.py --hotspots      # Show only hotspot analysis
    ./visualize_dependencies.py --threshold 5   # Show files affecting 5+ targets
"""

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path
from collections import defaultdict, deque
from typing import Dict, List, Set, Tuple, Optional
import re

try:
    import graphviz
    HAS_GRAPHVIZ = True
except ImportError:
    HAS_GRAPHVIZ = False
    print("Warning: graphviz not installed. Install with: pip install graphviz")
    print("Also ensure graphviz is installed on your system")

class DependencyAnalyzer:
    """Analyzes Meson build dependencies"""
    
    def __init__(self, build_dir: str = "build"):
        self.build_dir = Path(build_dir)
        self.intro_data = {}
        self.dependencies = defaultdict(set)  # file -> set of files that depend on it
        self.reverse_deps = defaultdict(set)  # file -> set of files it depends on
        self.target_deps = defaultdict(set)   # target -> set of source files
        self.file_to_targets = defaultdict(set)  # file -> set of targets
        
    def load_introspection_data(self):
        """Load Meson introspection data"""
        if not self.build_dir.exists():
            print(f"Error: Build directory '{self.build_dir}' does not exist.")
            print("Please run 'meson setup build' first.")
            sys.exit(1)
            
        # Try to get introspection data from Meson
        intro_types = [
            'targets',
            'dependencies', 
            'projectinfo',
            'buildsystem_files'
        ]
        
        has_introspection = False
        for intro_type in intro_types:
            try:
                result = subprocess.run(
                    ['meson', 'introspect', '--type', intro_type, str(self.build_dir)],
                    capture_output=True,
                    text=True,
                    check=True
                )
                self.intro_data[intro_type] = json.loads(result.stdout)
                has_introspection = True
            except subprocess.CalledProcessError:
                self.intro_data[intro_type] = []
            except FileNotFoundError:
                # Meson not in PATH
                self.intro_data[intro_type] = []
                
        # Fall back to compile_commands.json if introspection failed
        if not has_introspection:
            compile_commands_path = self.build_dir / 'compile_commands.json'
            if compile_commands_path.exists():
                print("Using compile_commands.json as fallback...")
                self._load_from_compile_commands(compile_commands_path)
            else:
                print("Warning: No introspection data available")
                
    def _load_from_compile_commands(self, path: Path):
        """Load source files from compile_commands.json"""
        try:
            with open(path, 'r') as f:
                commands = json.load(f)
                
            # Create pseudo-targets from compile commands
            targets = []
            files_by_dir = defaultdict(list)
            
            for cmd in commands:
                if 'file' in cmd:
                    source_file = cmd['file']
                    # Group by directory as pseudo-targets
                    dir_name = Path(source_file).parent.name
                    files_by_dir[dir_name].append(source_file)
                    self.file_to_targets[source_file].add(dir_name)
                    
            # Create target entries
            for dir_name, files in files_by_dir.items():
                targets.append({
                    'name': dir_name,
                    'sources': files
                })
                
            self.intro_data['targets'] = targets
            self.intro_data['projectinfo'] = {'name': 'uSEQ', 'version': 'unknown'}
        except Exception as e:
            print(f"Warning: Could not parse compile_commands.json: {e}")
                
    def analyze_dependencies(self):
        """Analyze dependencies from introspection data"""
        # Process targets and their source files
        for target in self.intro_data.get('targets', []):
            target_name = target['name']
            target_sources = []
            
            # Get source files for this target
            for source in target.get('sources', []):
                if isinstance(source, dict):
                    source_path = source.get('path', source.get('source', ''))
                else:
                    source_path = source
                    
                if source_path:
                    # Normalize path
                    source_path = Path(source_path).as_posix()
                    target_sources.append(source_path)
                    self.file_to_targets[source_path].add(target_name)
                    self.target_deps[target_name].add(source_path)
        
        # Analyze header dependencies by parsing source files
        self._analyze_header_dependencies()
        
    def _analyze_header_dependencies(self):
        """Parse source files to find header dependencies"""
        include_pattern = re.compile(r'#include\s*[<"]([^>"]+)[>"]')
        
        # Get all source files
        source_files = set()
        for files in self.file_to_targets.keys():
            if files.endswith(('.cpp', '.c', '.cc', '.cxx', '.h', '.hpp')):
                source_files.add(files)
        
        # Parse each source file for includes
        for source_file in source_files:
            full_path = Path(source_file)
            if not full_path.is_absolute():
                full_path = Path.cwd() / source_file
                
            if full_path.exists():
                try:
                    with open(full_path, 'r', encoding='utf-8', errors='ignore') as f:
                        content = f.read()
                        
                    # Find all includes
                    for match in include_pattern.finditer(content):
                        include_file = match.group(1)
                        
                        # Try to resolve the include path
                        resolved = self._resolve_include(include_file, full_path.parent)
                        if resolved:
                            # Normalize paths
                            dep_file = Path(resolved).relative_to(Path.cwd()).as_posix()
                            src_file = Path(source_file).as_posix()
                            
                            # Record dependency
                            self.dependencies[dep_file].add(src_file)
                            self.reverse_deps[src_file].add(dep_file)
                            
                except Exception as e:
                    pass  # Silently skip files we can't read
                    
    def _resolve_include(self, include_path: str, base_dir: Path) -> Optional[Path]:
        """Resolve an include path to an actual file"""
        # Common include directories to check
        include_dirs = [
            base_dir,
            Path.cwd() / 'uSEQ' / 'src',
            Path.cwd() / 'uSEQ' / 'src' / 'modulisp',
            Path.cwd() / 'uSEQ' / 'src' / 'modulisp' / 'lisp',
            Path.cwd() / 'uSEQ' / 'src' / 'utils',
            Path.cwd() / 'uSEQ' / 'src' / 'dsp',
            Path.cwd() / 'uSEQ' / 'src' / 'ports',
            Path.cwd() / 'test',
            Path.cwd(),
        ]
        
        for inc_dir in include_dirs:
            potential_path = inc_dir / include_path
            if potential_path.exists():
                return potential_path
                
        return None
        
    def find_hotspots(self, threshold: int = 3) -> List[Tuple[str, int]]:
        """Find files that trigger the most rebuilds when modified"""
        hotspots = []
        
        for file, dependents in self.dependencies.items():
            # Count how many targets are affected
            affected_targets = set()
            for dep in dependents:
                affected_targets.update(self.file_to_targets.get(dep, set()))
                
            if len(affected_targets) >= threshold:
                hotspots.append((file, len(affected_targets)))
                
        # Sort by impact
        hotspots.sort(key=lambda x: x[1], reverse=True)
        return hotspots
        
    def calculate_rebuild_impact(self, file: str) -> Set[str]:
        """Calculate all files that need rebuilding when this file changes"""
        to_rebuild = set()
        to_process = deque([file])
        processed = set()
        
        while to_process:
            current = to_process.popleft()
            if current in processed:
                continue
                
            processed.add(current)
            to_rebuild.add(current)
            
            # Add all files that depend on this one
            for dependent in self.dependencies.get(current, set()):
                if dependent not in processed:
                    to_process.append(dependent)
                    
        return to_rebuild
        
    def generate_dot_graph(self, output_file: str = "dependencies", 
                          format: str = "svg", 
                          focus_file: Optional[str] = None,
                          max_depth: int = 3):
        """Generate a Graphviz dot file for visualization"""
        if not HAS_GRAPHVIZ:
            print("Graphviz not available. Skipping graph generation.")
            return
            
        dot = graphviz.Digraph(comment='Build Dependencies', 
                              format=format,
                              engine='dot')
        
        # Configure graph appearance
        dot.attr(rankdir='LR', size='12,8', dpi='100')
        dot.attr('node', shape='box', style='rounded,filled', fillcolor='lightgray')
        
        # Determine which nodes to include
        nodes_to_include = set()
        edges_to_include = set()
        
        if focus_file:
            # Focus on a specific file and its dependencies
            nodes_to_include.add(focus_file)
            queue = deque([(focus_file, 0)])
            visited = set()
            
            while queue:
                current, depth = queue.popleft()
                if current in visited or depth > max_depth:
                    continue
                    
                visited.add(current)
                nodes_to_include.add(current)
                
                # Add dependencies and dependents
                for dep in self.dependencies.get(current, set()):
                    edges_to_include.add((current, dep))
                    nodes_to_include.add(dep)
                    if depth < max_depth:
                        queue.append((dep, depth + 1))
                        
                for dep in self.reverse_deps.get(current, set()):
                    edges_to_include.add((dep, current))
                    nodes_to_include.add(dep)
                    if depth < max_depth:
                        queue.append((dep, depth + 1))
        else:
            # Include all files with dependencies
            for file, deps in self.dependencies.items():
                if deps:  # Only include files with dependencies
                    nodes_to_include.add(file)
                    for dep in deps:
                        nodes_to_include.add(dep)
                        edges_to_include.add((file, dep))
        
        # Color code by file type
        for node in nodes_to_include:
            color = 'lightgray'
            if node.endswith('.h') or node.endswith('.hpp'):
                color = 'lightblue'
            elif node.endswith('.cpp') or node.endswith('.c'):
                color = 'lightgreen'
            elif 'test' in node.lower():
                color = 'lightyellow'
                
            # Shorten path for display
            display_name = Path(node).name
            if '/' in node:
                parent = Path(node).parent.name
                display_name = f"{parent}/{display_name}"
                
            dot.node(node, display_name, fillcolor=color)
        
        # Add edges
        for src, dst in edges_to_include:
            dot.edge(src, dst)
        
        # Render the graph
        output_path = f"{output_file}_{format}"
        try:
            dot.render(output_path, cleanup=True)
            print(f"Generated dependency graph: {output_path}.{format}")
        except Exception as e:
            print(f"Error generating graph: {e}")
            
    def print_analysis_report(self, threshold: int = 3):
        """Print a detailed analysis report"""
        print("\n" + "="*60)
        print("BUILD DEPENDENCY ANALYSIS REPORT")
        print("="*60)
        
        # Project info
        if 'projectinfo' in self.intro_data and self.intro_data['projectinfo']:
            info = self.intro_data['projectinfo']
            if isinstance(info, dict):
                print(f"\nProject: {info.get('name', 'Unknown')}")
                print(f"Version: {info.get('version', 'Unknown')}")
            elif isinstance(info, list) and len(info) > 0 and isinstance(info[0], dict):
                print(f"\nProject: {info[0].get('name', 'Unknown')}")
                print(f"Version: {info[0].get('version', 'Unknown')}")
            
        # Target summary
        print(f"\nTotal targets: {len(self.target_deps)}")
        print(f"Total source files: {len(self.file_to_targets)}")
        print(f"Total dependencies tracked: {sum(len(deps) for deps in self.dependencies.values())}")
        
        # Hotspot analysis
        print(f"\n{'='*60}")
        print(f"HOTSPOT ANALYSIS (files affecting {threshold}+ targets)")
        print(f"{'='*60}")
        
        hotspots = self.find_hotspots(threshold)
        if hotspots:
            print(f"\n{'File':<50} {'Targets Affected':<15}")
            print("-"*65)
            for file, count in hotspots[:20]:  # Show top 20
                display_path = file
                if len(display_path) > 48:
                    display_path = "..." + display_path[-45:]
                print(f"{display_path:<50} {count:<15}")
        else:
            print("No hotspots found with current threshold.")
            
        # Files with most dependencies
        print(f"\n{'='*60}")
        print("FILES WITH MOST DEPENDENCIES")
        print(f"{'='*60}")
        
        dep_counts = [(f, len(deps)) for f, deps in self.reverse_deps.items()]
        dep_counts.sort(key=lambda x: x[1], reverse=True)
        
        if dep_counts:
            print(f"\n{'File':<50} {'Dependencies':<15}")
            print("-"*65)
            for file, count in dep_counts[:10]:  # Show top 10
                display_path = file
                if len(display_path) > 48:
                    display_path = "..." + display_path[-45:]
                print(f"{display_path:<50} {count:<15}")
                
        # Circular dependency check
        print(f"\n{'='*60}")
        print("CIRCULAR DEPENDENCY CHECK")
        print(f"{'='*60}")
        
        circular = self._find_circular_dependencies()
        if circular:
            print("\nWarning: Circular dependencies detected!")
            for cycle in circular[:5]:  # Show first 5
                print(f"  -> {' -> '.join(Path(f).name for f in cycle)}")
        else:
            print("\nNo circular dependencies detected.")
            
    def _find_circular_dependencies(self) -> List[List[str]]:
        """Find circular dependencies in the build"""
        circular = []
        visited = set()
        
        def dfs(node: str, path: List[str], rec_stack: Set[str]):
            if node in rec_stack:
                # Found a cycle
                cycle_start = path.index(node)
                cycle = path[cycle_start:] + [node]
                if len(cycle) > 1:  # Ignore self-dependencies
                    circular.append(cycle)
                return
                
            if node in visited:
                return
                
            visited.add(node)
            rec_stack.add(node)
            
            for neighbor in self.dependencies.get(node, set()):
                dfs(neighbor, path + [node], rec_stack)
                
            rec_stack.remove(node)
            
        for node in self.dependencies.keys():
            if node not in visited:
                dfs(node, [], set())
                
        return circular
        
    def export_json(self, output_file: str = "dependencies.json"):
        """Export dependency data as JSON for further analysis"""
        data = {
            'dependencies': {k: list(v) for k, v in self.dependencies.items()},
            'reverse_dependencies': {k: list(v) for k, v in self.reverse_deps.items()},
            'file_to_targets': {k: list(v) for k, v in self.file_to_targets.items()},
            'target_dependencies': {k: list(v) for k, v in self.target_deps.items()},
            'hotspots': self.find_hotspots(),
        }
        
        with open(output_file, 'w') as f:
            json.dump(data, f, indent=2)
            
        print(f"\nExported dependency data to {output_file}")

def main():
    parser = argparse.ArgumentParser(
        description='Visualize build dependencies and identify compilation hotspots'
    )
    parser.add_argument(
        '--build-dir', 
        default='build',
        help='Meson build directory (default: build)'
    )
    parser.add_argument(
        '--format',
        choices=['svg', 'png', 'pdf', 'dot'],
        default='svg',
        help='Output format for graphs (default: svg)'
    )
    parser.add_argument(
        '--output',
        default='dependencies',
        help='Output file name prefix (default: dependencies)'
    )
    parser.add_argument(
        '--threshold',
        type=int,
        default=3,
        help='Hotspot threshold - files affecting N+ targets (default: 3)'
    )
    parser.add_argument(
        '--focus',
        help='Focus graph on a specific file and its dependencies'
    )
    parser.add_argument(
        '--depth',
        type=int,
        default=3,
        help='Maximum depth for focused graphs (default: 3)'
    )
    parser.add_argument(
        '--hotspots',
        action='store_true',
        help='Only show hotspot analysis'
    )
    parser.add_argument(
        '--json',
        action='store_true',
        help='Export dependency data as JSON'
    )
    parser.add_argument(
        '--no-graph',
        action='store_true',
        help='Skip graph generation'
    )
    
    args = parser.parse_args()
    
    # Create analyzer
    analyzer = DependencyAnalyzer(args.build_dir)
    
    print("Loading Meson introspection data...")
    analyzer.load_introspection_data()
    
    print("Analyzing dependencies...")
    analyzer.analyze_dependencies()
    
    # Generate outputs based on options
    if not args.hotspots:
        if not args.no_graph and HAS_GRAPHVIZ:
            print("Generating dependency graphs...")
            
            if args.focus:
                # Generate focused graph
                analyzer.generate_dot_graph(
                    f"{args.output}_focused",
                    args.format,
                    args.focus,
                    args.depth
                )
            else:
                # Generate full graph
                analyzer.generate_dot_graph(args.output, args.format)
                
                # Also generate hotspot graph
                hotspots = analyzer.find_hotspots(args.threshold)
                if hotspots and len(hotspots) > 0:
                    # Create a focused graph on the top hotspot
                    top_hotspot = hotspots[0][0]
                    analyzer.generate_dot_graph(
                        f"{args.output}_hotspot",
                        args.format,
                        top_hotspot,
                        2
                    )
    
    # Print analysis report
    analyzer.print_analysis_report(args.threshold)
    
    # Export JSON if requested
    if args.json:
        analyzer.export_json(f"{args.output}.json")
    
    print("\nDependency analysis complete!")
    
    # Provide additional suggestions
    if not HAS_GRAPHVIZ:
        print("\nTo enable graph generation, install graphviz:")
        print("  pip install graphviz")
        print("  # And install graphviz system package")
        print("  # On Ubuntu/Debian: sudo apt-get install graphviz")
        print("  # On macOS: brew install graphviz")
        print("  # On NixOS: nix-shell -p graphviz python3Packages.graphviz")

if __name__ == '__main__':
    main()