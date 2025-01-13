from fastapi import FastAPI, HTTPException
from fastapi.responses import FileResponse
from pathlib import Path

app = FastAPI()

# Đường dẫn lưu trữ file
FILE_DIRECTORY = Path("./files")
FILE_DIRECTORY.mkdir(exist_ok=True)  # Tạo thư mục nếu chưa tồn tại


@app.get("/")
def home():
    return {"message": "ESP32 File Download Server is Running"}


@app.get("/download/{file_name}")
def download_file(file_name: str):
    """
    API endpoint để tải file.
    """
    file_path = FILE_DIRECTORY / file_name

    if not file_path.exists():
        raise HTTPException(status_code=404, detail="File not found")

    return FileResponse(file_path, media_type="application/octet-stream", filename=file_name)
