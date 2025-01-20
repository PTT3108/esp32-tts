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
            percent = percent + 2;

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

function errorHandler(event) {
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

function computeCRC32(arrayBuffer) {
    const data = new Uint8Array(arrayBuffer);
    let crc = 0xFFFFFFFF;

    for (let i = 0; i < data.length; i++) {
        crc ^= data[i];
        for (let j = 0; j < 8; j++) {
            if ((crc & 1) === 1) {
                crc = (crc >>> 1) ^ 0xEDB88320;
            } else {
                crc >>>= 1;
            }
        }
    }

    // Đảo bit cuối
    crc ^= 0xFFFFFFFF;

    // Trả về chuỗi hex 8 ký tự (ví dụ "1a2b3c4d")
    return ("00000000" + (crc >>> 0).toString(16)).slice(-8);
}

// Hàm upload firmware
async function startUpdateProcess(deviceType) {
    const xhr = new XMLHttpRequest();
    const formData = new FormData();

    // Lấy file từ input
    const firmwareFile = document.getElementById("firmware_file").files[0];

    // Thiết lập listener cho xhr
    xhr.upload.addEventListener('progress', progressHandler, false);
    xhr.addEventListener('load', completeHandler, false);
    xhr.addEventListener('error', errorHandler, false);
    xhr.addEventListener('abort', abortHandler, false);

    // Mở kết nối
    xhr.open('POST', `/update/${deviceType}`);

    // Nếu không có file, gửi 0
    if (!firmwareFile) {
        formData.append('firmware', 0);
        formData.append('device', 0);
        formData.append('fileSize', 0);
        xhr.setRequestHeader('X-FileSize', 0);
        console.log("No firmware file selected.");
    } else {
        // Đọc toàn bộ file vào ArrayBuffer
        const fileBuffer = await firmwareFile.arrayBuffer();

        // Tính CRC-32
        const crcHex = computeCRC32(fileBuffer);

        // Thêm các trường vào FormData
        formData.append('firmware', firmwareFile);
        formData.append('device', deviceType);
        formData.append('fileSize', firmwareFile.size);
        formData.append('X-CRC32', crcHex);  // <-- Gửi kèm giá trị CRC

        // Set header dung lượng file
        xhr.setRequestHeader('X-FileSize', firmwareFile.size);
        xhr.setRequestHeader('X-CRC32', crcHex);

        // Debug thông tin
        console.log(`Name: ${firmwareFile.name}`);
        console.log(`Type: ${firmwareFile.type}`);
        console.log(`Size: ${firmwareFile.size} bytes`);
        console.log(`CRC-32: ${crcHex}`);
    }

    // Gửi form
    xhr.send(formData);
}


function getDeviceStates() {
    fetch("/getStates")
        .then(response => response.json())
        .then(data => {
            for (let i = 0; i < 5; i++) {
                const btn = document.getElementById('btn' + (i + 1));
                btn.innerText = data[i] ? "Turn OFF" : "Turn ON";
                btn.style.backgroundColor = data[i] ? "red" : "green";
            }
        });
}

function toggleDevice(index) {
    fetch(`/setState?index=${index}`, { method: 'GET' })
        .then(response => response.json())
        .then(data => {
            alert(data.message);
            getDeviceStates();
        });
}

// Tự động cập nhật trạng thái mỗi giây
setInterval(getDeviceStates, 1000);

// Lấy trạng thái ban đầu khi tải trang
window.onload = getDeviceStates;
@@include("libs.js")

