function _(el) {
    return document.getElementById(el);
}

// Hàm xử lý tiến độ upload
function progressHandler(event) {
    const percent = Math.round((event.loaded / event.total) * 100);
    _('progressBar').value = percent;
    _('status').innerHTML = percent + '% uploaded... please wait';
}

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
function completeHandler(event) {
    _('status').innerHTML = '';
    _('progressBar').value = 0;
    _('upload_btn').disabled = false
    const data = JSON.parse(event.target.responseText);
    if (data.status === 'ok') {
        function showMessage() {
            cuteAlert({
                type: 'success',
                title: 'Update Succeeded',
                message: data.msg
            });
        }
        let percent = 0;
        const interval = setInterval(() => {
            percent = percent + 1;

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
        cuteAlert({
            type: 'question',
            title: 'Targets Mismatch',
            message: data.msg,
            confirmText: 'Flash anyway',
            cancelText: 'Cancel'
        }).then((e) => {
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
                            message: 'An error occurred trying to force the update'
                        });
                    }
                }
            };
            xmlhttp.open('POST', '/forceupdate', true);
            const data = new FormData();
            data.append('action', e);
            xmlhttp.send(data);
        });
    } else {
        cuteAlert({
            type: 'error',
            title: 'Update Failed',
            message: data.msg
        });
    }
}

function  errorHandler(event) {
    _('status').innerHTML = '';
    _('progressBar').value = 0;
    _('upload_btn').disabled = false
    cuteAlert({
        type: 'error',
        title: 'Update Failed',
        message: event.target.responseText
    });
}


function abortHandler(event) {
    _('status').innerHTML = '';
    _('progressBar').value = 0;
    _('upload_btn').disabled = false
    cuteAlert({
        type: 'info',
        title: 'Update Aborted',
        message: event.target.responseText
    });
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
    
    // Theo dõi quá trình upload
    xhr.upload.addEventListener('progress', progressHandler, false);
    xhr.addEventListener('load', completeHandler, false);
    xhr.addEventListener('error', errorHandler, false);
    xhr.addEventListener('abort', abortHandler, false);
    xhr.open('POST', `/update/${deviceType}`);
    xhr.setRequestHeader('X-FileSize', firmwareFile.size);
    xhr.send(formData);
}

@@include("libs.js")

