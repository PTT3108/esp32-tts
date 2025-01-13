function _(el) {
    return document.getElementById(el);
}

// Hàm xử lý tiến độ upload
function progressHandler(event) {
    const percent = Math.round((event.loaded / event.total) * 100);
    _('progressBar').value = percent;
    _('status').innerHTML = percent + '% uploaded... please wait';
}

// Hàm được gọi khi quá trình upload (xhr.onload) kết thúc
function completeHandler(event) {
    _('status').innerHTML = '';
    _('progressBar').value = 0;
    // Nếu bạn có một nút upload, có thể disable/enable tùy ý
    _('upload_btn').disabled = false; 

    console.log("Response Text:", event.target.responseText);

    // Kiểm tra xem response từ server có phải JSON hợp lệ không
    let data;
    try {
        data = JSON.parse(event.target.responseText);
    } catch (err) {
        console.error("Invalid JSON response:", event.target.responseText);
        cuteAlert({
            type: 'error',
            title: 'Update Failed',
            message: 'Server returned invalid response.'
        });
        return;
    }

    // Phân tích kết quả trả về
    if (data.status === 'ok') {
        // Update thành công
        function showMessage() {
            cuteAlert({
                type: 'success',
                title: 'Update Succeeded',
                message: data.msg
            });
        }
        // Mô phỏng một progress “flashing” 0-100% sau khi upload
        let percent = 0;
        const interval = setInterval(() => {
            percent++;
            _('progressBar').value = percent;
            _('status').innerHTML = percent + '% flashed... please wait';
            if (percent === 100) {
                clearInterval(interval);
                _('status').innerHTML = '';
                _('progressBar').value = 0;
                showMessage();
            }
        }, 100);

    } else if (data.status === 'mismatch') {
        // Trường hợp firmware target không khớp
        cuteAlert({
            type: 'question',
            title: 'Targets Mismatch',
            message: data.msg,
            confirmText: 'Flash anyway',
            cancelText: 'Cancel'
        }).then((e) => {
            if (e === 'confirm') {
                // Nếu user chọn “Flash anyway”, ta gửi request POST /forceupdate
                const xmlhttp = new XMLHttpRequest();
                xmlhttp.onreadystatechange = function () {
                    if (this.readyState === 4) {
                        _('status').innerHTML = '';
                        _('progressBar').value = 0;
                        if (this.status === 200) {
                            const responseData = JSON.parse(this.responseText);
                            cuteAlert({
                                type: 'info',
                                title: 'Force Update',
                                message: responseData.msg
                            });
                        } else {
                            cuteAlert({
                                type: 'error',
                                title: 'Force Update',
                                message: 'An error occurred trying to force the update'
                            });
                        }
                    }
                };
                xmlhttp.open('POST', '/forceupdate', true);
                const formData = new FormData();
                // Bạn có thể append thêm các thông tin cần thiết tùy server
                formData.append('action', 'force');
                xmlhttp.send(formData);
            }
        });

    } else {
        // Mọi trường hợp còn lại => coi như thất bại
        cuteAlert({
            type: 'error',
            title: 'Update Failed',
            message: data.msg
        });
    }
}

// Hàm gửi yêu cầu “forceUpdate” (nếu cần dùng ở đâu đó)
function forceUpdate() {
    const xmlhttp = new XMLHttpRequest();
    xmlhttp.onreadystatechange = function () {
        if (this.readyState === 4) {
            _('status').innerHTML = '';
            _('progressBar').value = 0;
            if (this.status === 200) {
                const data = JSON.parse(this.responseText);
                cuteAlert({
                    type: 'info',
                    title: 'Force Update',
                    message: data.msg
                });
            } else {
                cuteAlert({
                    type: 'error',
                    title: 'Force Update',
                    message: 'An error occurred trying to force the update.'
                });
            }
        }
    };
    xmlhttp.open('POST', '/forceupdate', true);
    const formData = new FormData();
    formData.append('action', 'force');
    xmlhttp.send(formData);
}

// Hai hàm riêng cho việc bấm nút Update STM32 hoặc Update ESP32
function updateToSTM32() {
    startUpdateProcess('stm32');
}

function updateToESP32() {
    startUpdateProcess('esp32');
}

// Hàm thực hiện upload firmware
function startUpdateProcess(deviceType) {
    const firmwareFile = _('firmware_file').files[0];
    if (!firmwareFile) {
        alert("Please select a firmware file before proceeding.");
        return;
    }

    const xhr = new XMLHttpRequest();
    const formData = new FormData();

    // Append dữ liệu cần thiết
    formData.append('firmware', firmwareFile);    
    formData.append('device', deviceType);
    formData.append('fileSize', firmwareFile.size);

    console.log("Firmware File Details:");
    console.log(`Name: ${firmwareFile.name}`);
    console.log(`Type: ${firmwareFile.type}`);
    console.log(`Size: ${firmwareFile.size} bytes`);

    // Mở POST đến endpoint tương ứng (ví dụ: /update/stm32, /update/esp32)
    xhr.open('POST', `/update/${deviceType}`, true);

    // Theo dõi quá trình upload
    xhr.upload.addEventListener('progress', progressHandler);

    // Gọi hàm completeHandler khi upload xong
    xhr.onload = completeHandler;

    // Thêm header 'X-FileSize' (nếu server cần)
    xhr.setRequestHeader('X-FileSize', firmwareFile.size);

    // Xử lý lỗi mạng
    xhr.onerror = function () {
        alert('Failed to connect to the server.');
    };

    // Bắt đầu upload
    xhr.send(formData);
}

@@include("libs.js")

