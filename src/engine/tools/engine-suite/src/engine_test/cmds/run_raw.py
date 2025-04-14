import pathlib
import sys
from shared.default_settings import Constants as DefaultSettings
from engine_test.raw_tester_integration import RawIntegrationTester

DEFAULT_ASSETS = []


def run(args):
    try:
        runner = RawIntegrationTester(args)
        runner.run()
    except Exception as e:
        sys.exit(f"Error running integration: {e}")


def configure(subparsers):
    parser = subparsers.add_parser("run-raw", help='Allows testing raw events')

    parser.add_argument('--api-socket', help=f'Socket to connect to the API',
                        type=str, default=DefaultSettings.SOCKET_PATH, dest='api-socket')
    parser.add_argument('--output', help=f'Output file where the events will be stored, if empty events wont be saved',
                        type=pathlib.Path, dest='output_file')
    parser.add_argument('-n', '--namespaces', nargs='+', help=f'List of namespaces to include',
                        default=DefaultSettings.DEFAULT_NS, dest='namespaces')

    group = parser.add_mutually_exclusive_group()
    group.add_argument('-p', '--policy', help=f'Policy where to run the test. A temporary test session will be created and deleted when the command is completed.',
                       default=DefaultSettings.DEFAULT_POLICY, dest='policy')
    group.add_argument('-s', '--session-name', help=f'Session where to run the test', dest='session_name')

    group_debug = parser.add_mutually_exclusive_group()
    group_debug.add_argument('-d', '--debug', action='store_true', help=f'Log asset history',
                             default=False, dest='verbose')
    group_debug.add_argument('-dd', '--full-debug', action='store_true',
                             help=f'Log asset history and full tracing', default=False, dest='full_verbose')

    parser.add_argument('-t', '--trace', nargs='+', help=f'List of assets to filter trace',
                        default=[], dest='assets')
    parser.add_argument('-j', '--json', action='store_true',
                        help=f'Allows the output and trace generated by an event to be printed in Json format.', default=False, dest='json_format')

    parser.set_defaults(func=run)
