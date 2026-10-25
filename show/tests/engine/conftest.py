"""pytest hooks for show/tests/engine."""


def pytest_configure(config):
    config.addinivalue_line("markers", "slow: builds show.exe variants or runs the emulator")
