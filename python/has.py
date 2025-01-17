import tkinter as tk
from tkinter import filedialog
import hashlib

def compute_sha256_file(file_path: str) -> str:
    """
    Tính SHA-256 cho file ở đường dẫn `file_path`.
    Trả về chuỗi hex (64 ký tự).
    """
    sha256_hash = hashlib.sha256()
    
    # Mở file ở chế độ nhị phân
    with open(file_path, "rb") as f:
        # Đọc file theo từng khối để tránh tốn nhiều RAM
        for block in iter(lambda: f.read(4096), b""):
            sha256_hash.update(block)
    
    return sha256_hash.hexdigest()

if __name__ == "__main__":
    # Tạo root để dùng filedialog, nhưng ẩn cửa sổ chính
    root = tk.Tk()
    root.withdraw()
    
    # Hộp thoại chọn file
    file_path = filedialog.askopenfilename(
        title="Chọn file để tính SHA-256"
    )
    
    if file_path:
        hash_value = compute_sha256_file(file_path)
        print(f"Đường dẫn file: {file_path}")
        print(f"SHA-256: {hash_value}")
    else:
        print("Không có file nào được chọn.")
