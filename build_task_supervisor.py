#!/usr/bin/env python3
"""
Build Task Supervisor for uSEQ Build Optimization

This script continuously launches Claude CLI to execute build optimization tasks
from BUILD_OPTIMIZATION_TODO.md, verifying completion and commits after each task.
"""

import subprocess
import time
import re
import sys
import os
from datetime import datetime
from pathlib import Path

class BuildTaskSupervisor:
    def __init__(self):
        self.working_dir = Path.cwd()
        self.todo_file = self.working_dir / "BUILD_OPTIMIZATION_TODO.md"
        self.log_file = self.working_dir / "build_supervisor.log"
        self.max_attempts_per_task = 3
        self.task_timeout = 1800  # 30 minutes per task
        self.completed_tasks = []
        
    def log(self, message):
        """Log message to both console and file"""
        timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        log_entry = f"[{timestamp}] {message}"
        print(log_entry)
        with open(self.log_file, "a") as f:
            f.write(log_entry + "\n")
    
    def get_uncompleted_tasks(self):
        """Parse BUILD_OPTIMIZATION_TODO.md and return list of uncompleted tasks"""
        uncompleted = []
        with open(self.todo_file, "r") as f:
            content = f.read()
        
        # Match task patterns like "### BUILD-002: ..." or "### INTERFACE-001: ..."
        # Check if the checkbox is unchecked "- [ ]"
        pattern = r'### ([A-Z]+-\d{3}): (.+?)\n- \[ \]'
        matches = re.finditer(pattern, content, re.MULTILINE)
        
        for match in matches:
            task_id = match.group(1)
            task_name = match.group(2)
            uncompleted.append((task_id, task_name))
        
        return uncompleted
    
    def get_latest_commit_hash(self):
        """Get the latest git commit hash"""
        try:
            result = subprocess.run(
                ["git", "rev-parse", "HEAD"],
                capture_output=True,
                text=True,
                cwd=self.working_dir,
                timeout=10
            )
            return result.stdout.strip() if result.returncode == 0 else None
        except Exception as e:
            self.log(f"Error getting commit hash: {e}")
            return None
    
    def verify_task_completion(self, task_id, prev_commit_hash):
        """Verify that a task was marked complete and committed"""
        # Check if task is marked complete in TODO file
        with open(self.todo_file, "r") as f:
            content = f.read()
        
        # Look for the task with a checked checkbox
        pattern = rf'### {re.escape(task_id)}: .+?\n- \[x\]'
        if not re.search(pattern, content, re.MULTILINE):
            return False, "Task not marked as complete in BUILD_OPTIMIZATION_TODO.md"
        
        # Check if a new commit was made
        current_commit_hash = self.get_latest_commit_hash()
        if current_commit_hash == prev_commit_hash:
            return False, "No new commit detected"
        
        # Verify commit message contains task ID
        try:
            result = subprocess.run(
                ["git", "log", "-1", "--pretty=%s"],
                capture_output=True,
                text=True,
                cwd=self.working_dir,
                timeout=10
            )
            commit_message = result.stdout.strip()
            if task_id not in commit_message:
                self.log(f"Warning: Commit message doesn't contain task ID {task_id}: {commit_message}")
        except Exception as e:
            self.log(f"Error checking commit message: {e}")
        
        return True, "Task completed successfully"
    
    def execute_claude_task(self):
        """Execute Claude with /next-build-task command"""
        self.log("Launching Claude to execute next build task...")
        
        try:
            # Use npx to run claude with --print for non-interactive mode and --dangerously-skip-permissions for automation
            result = subprocess.run(
                ["npx", "@anthropic-ai/claude-code", "--print", "--dangerously-skip-permissions", "/next-build-task"],
                capture_output=True,
                text=True,
                cwd=self.working_dir,
                timeout=self.task_timeout
            )
            
            if result.returncode != 0:
                self.log(f"Claude exited with error code {result.returncode}")
                self.log(f"Stderr: {result.stderr[:1000]}")  # Log first 1000 chars of error
                return False
            
            # Log a summary of the output (last 500 chars to see completion message)
            output_summary = result.stdout[-500:] if len(result.stdout) > 500 else result.stdout
            self.log(f"Claude output summary: ...{output_summary}")
            
            return True
            
        except subprocess.TimeoutExpired:
            self.log(f"Claude execution timed out after {self.task_timeout} seconds")
            return False
        except Exception as e:
            self.log(f"Error executing Claude: {e}")
            return False
    
    def run(self):
        """Main supervisor loop"""
        self.log("=" * 60)
        self.log("Starting Build Task Supervisor")
        self.log(f"Working directory: {self.working_dir}")
        self.log(f"TODO file: {self.todo_file}")
        self.log("=" * 60)
        
        iteration = 0
        consecutive_failures = 0
        max_consecutive_failures = 3
        
        while True:
            iteration += 1
            self.log(f"\n--- Iteration {iteration} ---")
            
            # Get list of uncompleted tasks
            uncompleted_tasks = self.get_uncompleted_tasks()
            
            if not uncompleted_tasks:
                self.log("SUCCESS: All tasks completed!")
                self.log(f"Total completed tasks: {len(self.completed_tasks)}")
                for task_id, task_name in self.completed_tasks:
                    self.log(f"  - {task_id}: {task_name}")
                break
            
            self.log(f"Found {len(uncompleted_tasks)} uncompleted tasks")
            next_task = uncompleted_tasks[0]
            self.log(f"Next task: {next_task[0]} - {next_task[1]}")
            
            # Get current commit hash before execution
            prev_commit_hash = self.get_latest_commit_hash()
            
            # Execute Claude
            success = self.execute_claude_task()
            
            if not success:
                consecutive_failures += 1
                self.log(f"Task execution failed (consecutive failures: {consecutive_failures})")
                
                if consecutive_failures >= max_consecutive_failures:
                    self.log("ERROR: Too many consecutive failures. Stopping supervisor.")
                    sys.exit(1)
                
                # Wait before retry
                self.log("Waiting 30 seconds before retry...")
                time.sleep(30)
                continue
            
            # Reset failure counter on successful execution
            consecutive_failures = 0
            
            # Wait a bit for file system and git to settle
            time.sleep(5)
            
            # Verify task completion
            completed, message = self.verify_task_completion(next_task[0], prev_commit_hash)
            
            if completed:
                self.log(f"✓ Task {next_task[0]} completed successfully: {message}")
                self.completed_tasks.append(next_task)
                
                # Brief pause between tasks
                self.log("Waiting 10 seconds before next task...")
                time.sleep(10)
            else:
                self.log(f"✗ Task {next_task[0]} verification failed: {message}")
                self.log("Stopping supervisor for manual intervention.")
                sys.exit(1)
        
        self.log("\n" + "=" * 60)
        self.log("Build Task Supervisor completed successfully!")
        self.log("=" * 60)

if __name__ == "__main__":
    supervisor = BuildTaskSupervisor()
    try:
        supervisor.run()
    except KeyboardInterrupt:
        supervisor.log("\nSupervisor interrupted by user")
        sys.exit(0)
    except Exception as e:
        supervisor.log(f"Unexpected error: {e}")
        sys.exit(1)