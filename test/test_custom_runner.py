from pathlib import Path
import subprocess

import click

from platformio.test.result import TestCase, TestStatus
from platformio.test.runners.base import TestRunnerBase


class CustomTestRunner(TestRunnerBase):
    def setup(self):
        self.project_dir = Path(__file__).resolve().parents[1]
        self.build_dir = self.project_dir / "build"
        self.binary_path = self.build_dir / "test" / "test_json_protocol"
        self.build_target = "test/test_json_protocol"

    def stage_building(self):
        if self.options.without_building:
            return None

        click.secho("Building native JSON protocol tests...", bold=True)
        result = subprocess.run(
            ["ninja", "-C", str(self.build_dir), self.build_target],
            cwd=self.project_dir,
            capture_output=True,
            text=True,
            check=False,
        )
        if result.stdout:
            click.echo(result.stdout, nl=False)
        if result.stderr:
            click.echo(result.stderr, err=True, nl=False)
        if result.returncode != 0:
            raise RuntimeError(
                f"Failed to build {self.build_target} in {self.build_dir}"
            )

    def stage_uploading(self):
        return None

    def stage_testing(self):
        if self.options.without_testing:
            return None

        click.secho("Testing...", bold=True)
        if not self.binary_path.is_file():
            raise RuntimeError(f"Missing native test binary: {self.binary_path}")

        result = subprocess.run(
            [str(self.binary_path)],
            cwd=self.project_dir,
            capture_output=True,
            text=True,
            check=False,
        )
        output = "".join(part for part in (result.stdout, result.stderr) if part)
        if output:
            click.echo(output, nl=not output.endswith("\n"))

        self.test_suite.add_case(
            TestCase(
                name="native:test_json_protocol",
                status=TestStatus.PASSED
                if result.returncode == 0
                else TestStatus.FAILED,
                message=None
                if result.returncode == 0
                else f"{self.binary_path} exited with {result.returncode}",
                stdout=output.strip() or None,
            )
        )
