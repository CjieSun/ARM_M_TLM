#!/usr/bin/env python3
"""
Comprehensive test runner for ARM_M_TLM simulator
Runs all tests and generates detailed reports in multiple formats
"""

import os
import sys
import json
import subprocess
import re
import datetime
import argparse
from pathlib import Path
from dataclasses import dataclass, asdict
from typing import List, Dict, Optional, Any

@dataclass
class TestMetrics:
    """Performance metrics for a single test"""
    instructions_executed: int = 0
    memory_reads: int = 0
    memory_writes: int = 0
    register_reads: int = 0
    register_writes: int = 0
    branches_taken: int = 0
    irq_count: int = 0
    simulation_time: str = "0 ms"
    instructions_per_second: int = 0

@dataclass
class TestResult:
    """Result of a single test execution"""
    name: str
    hex_file: str
    status: str  # "PASS", "FAIL", "TIMEOUT", "ERROR"
    duration: float  # execution time in seconds
    metrics: TestMetrics
    output: str = ""
    error: str = ""

class TestRunner:
    """Main test runner class"""
    
    def __init__(self, simulator_path: str, test_dir: str):
        self.simulator_path = Path(simulator_path)
        self.test_dir = Path(test_dir)
        self.results: List[TestResult] = []
        
        if not self.simulator_path.exists():
            raise FileNotFoundError(f"Simulator not found: {simulator_path}")
        if not self.test_dir.exists():
            raise FileNotFoundError(f"Test directory not found: {test_dir}")
    
    def find_test_files(self) -> List[Path]:
        """Find all .hex test files in the test directory"""
        return sorted(self.test_dir.glob("*.hex"))
    
    def parse_performance_report(self, output: str) -> TestMetrics:
        """Extract performance metrics from simulator output"""
        metrics = TestMetrics()
        
        # Performance report section parsing
        perf_section = re.search(r'=== Performance Report ===.*?=========================', output, re.DOTALL)
        if not perf_section:
            return metrics
        
        perf_text = perf_section.group(0)
        
        # Extract metrics using regex
        patterns = {
            'instructions_executed': r'Instructions executed: (\d+)',
            'memory_reads': r'Memory reads: (\d+)',
            'memory_writes': r'Memory writes: (\d+)',
            'register_reads': r'Register reads: (\d+)',
            'register_writes': r'Register writes: (\d+)',
            'branches_taken': r'Branches taken: (\d+)',
            'irq_count': r'IRQ count: (\d+)',
            'simulation_time': r'Simulation time: ([^\n]+)',
            'instructions_per_second': r'Instructions per second: (\d+)'
        }
        
        for field, pattern in patterns.items():
            match = re.search(pattern, perf_text)
            if match:
                value = match.group(1)
                if field in ['simulation_time']:
                    setattr(metrics, field, value.strip())
                else:
                    setattr(metrics, field, int(value))
        
        return metrics
    
    def run_single_test(self, hex_file: Path, timeout: int = 60) -> TestResult:
        """Run a single test and capture results"""
        test_name = hex_file.stem
        
        print(f"Running test: {test_name}...")
        
        start_time = datetime.datetime.now()
        
        try:
            # Run the simulator
            cmd = [str(self.simulator_path.absolute()), "--hex", str(hex_file.absolute()), "--debug"]
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=timeout,
                cwd=self.test_dir.parent
            )
            
            end_time = datetime.datetime.now()
            duration = (end_time - start_time).total_seconds()
            
            # Parse output for metrics
            metrics = self.parse_performance_report(result.stdout)
            
            # Determine test status
            status = "PASS"
            if result.returncode != 0:
                status = "FAIL"
            elif "ERROR" in result.stdout or "Failed" in result.stdout:
                status = "FAIL"
            
            return TestResult(
                name=test_name,
                hex_file=str(hex_file),
                status=status,
                duration=duration,
                metrics=metrics,
                output=result.stdout,
                error=result.stderr
            )
            
        except subprocess.TimeoutExpired:
            end_time = datetime.datetime.now()
            duration = (end_time - start_time).total_seconds()
            
            return TestResult(
                name=test_name,
                hex_file=str(hex_file),
                status="TIMEOUT",
                duration=duration,
                metrics=TestMetrics(),
                output="",
                error=f"Test timed out after {timeout} seconds"
            )
            
        except Exception as e:
            end_time = datetime.datetime.now()
            duration = (end_time - start_time).total_seconds()
            
            return TestResult(
                name=test_name,
                hex_file=str(hex_file),
                status="ERROR",
                duration=duration,
                metrics=TestMetrics(),
                output="",
                error=str(e)
            )
    
    def run_all_tests(self, timeout: int = 60) -> None:
        """Run all tests found in the test directory"""
        hex_files = self.find_test_files()
        
        print(f"Found {len(hex_files)} test files")
        print("=" * 50)
        
        self.results = []
        for hex_file in hex_files:
            result = self.run_single_test(hex_file, timeout)
            self.results.append(result)
            
            # Print brief result
            status_symbol = "✓" if result.status == "PASS" else "✗"
            print(f"{status_symbol} {result.name}: {result.status} ({result.duration:.2f}s)")
        
        print("=" * 50)
        print(f"Tests completed: {len(self.results)}")
    
    def generate_summary(self) -> Dict[str, Any]:
        """Generate test run summary statistics"""
        total_tests = len(self.results)
        passed_tests = sum(1 for r in self.results if r.status == "PASS")
        failed_tests = sum(1 for r in self.results if r.status == "FAIL")
        error_tests = sum(1 for r in self.results if r.status == "ERROR")
        timeout_tests = sum(1 for r in self.results if r.status == "TIMEOUT")
        
        total_duration = sum(r.duration for r in self.results)
        total_instructions = sum(r.metrics.instructions_executed for r in self.results)
        total_memory_ops = sum(r.metrics.memory_reads + r.metrics.memory_writes for r in self.results)
        
        return {
            "timestamp": datetime.datetime.now().isoformat(),
            "total_tests": total_tests,
            "passed": passed_tests,
            "failed": failed_tests,
            "errors": error_tests,
            "timeouts": timeout_tests,
            "success_rate": (passed_tests / total_tests * 100) if total_tests > 0 else 0,
            "total_duration": total_duration,
            "total_instructions_executed": total_instructions,
            "total_memory_operations": total_memory_ops,
            "average_duration": total_duration / total_tests if total_tests > 0 else 0
        }
    
    def save_json_report(self, output_path: Path) -> None:
        """Save detailed test results as JSON"""
        report_data = {
            "summary": self.generate_summary(),
            "tests": [asdict(result) for result in self.results]
        }
        
        with open(output_path, 'w', encoding='utf-8') as f:
            json.dump(report_data, f, indent=2, ensure_ascii=False)
        
        print(f"JSON report saved to: {output_path}")
    
    def save_html_report(self, output_path: Path) -> None:
        """Generate HTML test report"""
        summary = self.generate_summary()
        
        html_content = self._generate_html_template(summary)
        
        with open(output_path, 'w', encoding='utf-8') as f:
            f.write(html_content)
        
        print(f"HTML report saved to: {output_path}")
    
    def _generate_html_template(self, summary: Dict[str, Any]) -> str:
        """Generate HTML report template"""
        # Generate test results table
        test_rows = ""
        for result in self.results:
            status_class = result.status.lower()
            status_icon = "✓" if result.status == "PASS" else "✗"
            
            test_rows += f"""
            <tr class="test-{status_class}">
                <td>{status_icon} {result.name}</td>
                <td><span class="status status-{status_class}">{result.status}</span></td>
                <td>{result.duration:.3f}s</td>
                <td>{result.metrics.instructions_executed:,}</td>
                <td>{result.metrics.memory_reads:,}</td>
                <td>{result.metrics.memory_writes:,}</td>
                <td>{result.metrics.branches_taken:,}</td>
                <td>{result.metrics.instructions_per_second:,}</td>
            </tr>"""
        
        # Generate summary cards
        total_memory_ops = summary["total_memory_operations"]
        
        return f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ARM_M_TLM Test Report</title>
    <style>
        body {{ font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; margin: 0; padding: 20px; background-color: #f5f5f5; }}
        .container {{ max-width: 1200px; margin: 0 auto; }}
        .header {{ background: linear-gradient(135deg, #667eea 0%, #764ba2 100%); color: white; padding: 30px; border-radius: 10px; margin-bottom: 30px; }}
        .header h1 {{ margin: 0; font-size: 2.5em; }}
        .header p {{ margin: 10px 0 0 0; opacity: 0.9; }}
        
        .summary {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 20px; margin-bottom: 30px; }}
        .summary-card {{ background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); text-align: center; }}
        .summary-card h3 {{ margin: 0; font-size: 2em; }}
        .summary-card p {{ margin: 10px 0 0 0; color: #666; }}
        
        .success-rate {{ color: #28a745; }}
        .total-tests {{ color: #17a2b8; }}
        .failed-tests {{ color: #dc3545; }}
        .duration {{ color: #6f42c1; }}
        
        .test-results {{ background: white; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); overflow: hidden; }}
        .test-results h2 {{ margin: 0; padding: 20px; background: #f8f9fa; border-bottom: 1px solid #dee2e6; }}
        
        table {{ width: 100%; border-collapse: collapse; }}
        th, td {{ padding: 12px; text-align: left; border-bottom: 1px solid #dee2e6; }}
        th {{ background: #f8f9fa; font-weight: 600; }}
        
        .status {{ padding: 4px 8px; border-radius: 4px; font-weight: bold; font-size: 0.9em; }}
        .status-pass {{ background: #d4edda; color: #155724; }}
        .status-fail {{ background: #f8d7da; color: #721c24; }}
        .status-error {{ background: #fff3cd; color: #856404; }}
        .status-timeout {{ background: #d1ecf1; color: #0c5460; }}
        
        .test-pass {{ background-color: #f8fff8; }}
        .test-fail {{ background-color: #fff8f8; }}
        .test-error {{ background-color: #fffcf0; }}
        .test-timeout {{ background-color: #f0f8ff; }}
        
        .footer {{ margin-top: 30px; text-align: center; color: #666; }}
    </style>
</head>
<body>
    <div class="container">
        <div class="header">
            <h1>ARM_M_TLM Test Report</h1>
            <p>Generated on {summary['timestamp']}</p>
        </div>
        
        <div class="summary">
            <div class="summary-card">
                <h3 class="success-rate">{summary['success_rate']:.1f}%</h3>
                <p>Success Rate</p>
            </div>
            <div class="summary-card">
                <h3 class="total-tests">{summary['total_tests']}</h3>
                <p>Total Tests</p>
            </div>
            <div class="summary-card">
                <h3 class="failed-tests">{summary['failed'] + summary['errors'] + summary['timeouts']}</h3>
                <p>Failed Tests</p>
            </div>
            <div class="summary-card">
                <h3 class="duration">{summary['total_duration']:.1f}s</h3>
                <p>Total Duration</p>
            </div>
            <div class="summary-card">
                <h3>{summary['total_instructions_executed']:,}</h3>
                <p>Instructions Executed</p>
            </div>
            <div class="summary-card">
                <h3>{total_memory_ops:,}</h3>
                <p>Memory Operations</p>
            </div>
        </div>
        
        <div class="test-results">
            <h2>Test Results Details</h2>
            <table>
                <thead>
                    <tr>
                        <th>Test Name</th>
                        <th>Status</th>
                        <th>Duration</th>
                        <th>Instructions</th>
                        <th>Mem Reads</th>
                        <th>Mem Writes</th>
                        <th>Branches</th>
                        <th>IPS</th>
                    </tr>
                </thead>
                <tbody>
                    {test_rows}
                </tbody>
            </table>
        </div>
        
        <div class="footer">
            <p>ARM Cortex-M0 SystemC-TLM Simulator Test Report</p>
        </div>
    </div>
</body>
</html>"""
    
    def print_summary(self) -> None:
        """Print test summary to console"""
        summary = self.generate_summary()
        
        print("\n" + "=" * 50)
        print("TEST SUMMARY")
        print("=" * 50)
        print(f"Total tests: {summary['total_tests']}")
        print(f"Passed: {summary['passed']}")
        print(f"Failed: {summary['failed']}")
        print(f"Errors: {summary['errors']}")
        print(f"Timeouts: {summary['timeouts']}")
        print(f"Success rate: {summary['success_rate']:.1f}%")
        print(f"Total duration: {summary['total_duration']:.2f}s")
        print(f"Average duration: {summary['average_duration']:.2f}s")
        print(f"Total instructions executed: {summary['total_instructions_executed']:,}")
        print(f"Total memory operations: {summary['total_memory_operations']:,}")


def main():
    parser = argparse.ArgumentParser(description="ARM_M_TLM comprehensive test runner")
    parser.add_argument("--simulator", required=True, help="Path to ARM_M_TLM simulator executable")
    parser.add_argument("--test-dir", required=True, help="Path to test directory containing .hex files")
    parser.add_argument("--output-dir", default="./reports", help="Output directory for reports")
    parser.add_argument("--timeout", type=int, default=60, help="Timeout per test in seconds")
    parser.add_argument("--html", action="store_true", help="Generate HTML report")
    parser.add_argument("--json", action="store_true", help="Generate JSON report")
    
    args = parser.parse_args()
    
    # Create output directory
    output_dir = Path(args.output_dir)
    output_dir.mkdir(parents=True, exist_ok=True)
    
    # Create test runner
    try:
        runner = TestRunner(args.simulator, args.test_dir)
    except FileNotFoundError as e:
        print(f"Error: {e}")
        sys.exit(1)
    
    # Run all tests
    runner.run_all_tests(timeout=args.timeout)
    
    # Print summary
    runner.print_summary()
    
    # Generate reports
    timestamp = datetime.datetime.now().strftime("%Y%m%d_%H%M%S")
    
    if args.json:
        json_path = output_dir / f"test_report_{timestamp}.json"
        runner.save_json_report(json_path)
    
    if args.html:
        html_path = output_dir / f"test_report_{timestamp}.html"
        runner.save_html_report(html_path)
    
    # Default: always generate both reports
    if not args.json and not args.html:
        json_path = output_dir / f"test_report_{timestamp}.json"
        html_path = output_dir / f"test_report_{timestamp}.html"
        runner.save_json_report(json_path)
        runner.save_html_report(html_path)
    
    # Exit with appropriate code
    summary = runner.generate_summary()
    if summary["failed"] > 0 or summary["errors"] > 0:
        sys.exit(1)
    else:
        sys.exit(0)


if __name__ == "__main__":
    main()