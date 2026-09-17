"""Stable errors shared by the preview frontend and build driver."""


class PreviewError(Exception):
    def __init__(self, status: int, code: str, message: str):
        super().__init__(message)
        self.status = status
        self.code = code
