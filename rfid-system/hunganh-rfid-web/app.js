// Import functions from the Firebase SDK (Modular v9)
import { initializeApp } from "https://www.gstatic.com/firebasejs/10.8.0/firebase-app.js";
import { getFirestore, collection, onSnapshot, query, orderBy, limit, addDoc, serverTimestamp } from "https://www.gstatic.com/firebasejs/10.8.0/firebase-firestore.js";

// Firebase Configuration (Trích xuất từ app cũ hunganh-doam1)
// Do dùng Firestore chung project nên cấu hình giống nhau
const firebaseConfig = {
  apiKey: "AIzaSyBoVEofW3IdbppeDdP9ksiIoA_zac0zS5U",
  authDomain: "hunganh-doam1.firebaseapp.com",
  projectId: "hunganh-doam1",
  storageBucket: "hunganh-doam1.appspot.com",
  messagingSenderId: "123456789", // Placeholder
  appId: "1:123456789:web:abcdef123456" // Placeholder
};

// Khởi tạo Firebase
const app = initializeApp(firebaseConfig);
const db = getFirestore(app);

// DOM Elements
const logsTableBody = document.getElementById('logs-table-body');
const btnOpenDoor = document.getElementById('btn-open-door');
const doorStatus = document.getElementById('door-status');

// 1. Lắng nghe cập nhật bảng Logs Realtime
const logsQuery = query(collection(db, "logs"), orderBy("timestamp", "desc"), limit(20));

onSnapshot(logsQuery, (snapshot) => {
    logsTableBody.innerHTML = ''; // Xóa bảng cũ
    let count = 0;
    
    snapshot.forEach((doc) => {
        const data = doc.data();
        count++;

        // Xử lý thời gian (Firebase Timestamp -> JS Date)
        let timeStr = "Đang tải...";
        if (data.timestamp) {
             const date = data.timestamp.toDate();
             timeStr = date.toLocaleString('vi-VN');
        } else {
             // Trường hợp ESP32 mới push lên chưa có serverTimestamp
             timeStr = new Date().toLocaleString('vi-VN') + " (Vừa xong)";
        }

        // Tạo thẻ màu cho trạng thái
        let statusBadge = `<span class="px-2 py-1 bg-emerald-500/20 text-emerald-400 rounded-full text-xs font-semibold">Đã Cấp Quyền</span>`;
        if(data.status && data.status.includes("Tu choi")) {
             statusBadge = `<span class="px-2 py-1 bg-rose-500/20 text-rose-400 rounded-full text-xs font-semibold">Từ Chối</span>`;
        }

        const tr = document.createElement('tr');
        tr.className = "hover:bg-white/5 transition-colors";
        tr.innerHTML = `
            <td class="p-4 whitespace-nowrap text-slate-300">${timeStr}</td>
            <td class="p-4 font-semibold text-white">${data.name || 'Khách lạ'}</td>
            <td class="p-4"><code class="px-2 py-1 bg-slate-800 rounded font-mono text-xs text-sky-400">${data.uid || 'N/A'}</code></td>
            <td class="p-4">${statusBadge}</td>
        `;
        logsTableBody.appendChild(tr);
    });

    if (count === 0) {
        logsTableBody.innerHTML = `<tr><td colspan="4" class="p-4 text-center text-slate-500 italic">Chưa có lịch sử quẹt thẻ nào.</td></tr>`;
    }
});


// 2. Chức năng Mở cửa từ xa
btnOpenDoor.addEventListener('click', async () => {
    try {
        // Vô hiệu hóa nút tạm thời chống bấm 2 lần
        btnOpenDoor.disabled = true;
        btnOpenDoor.innerHTML = `<svg class="animate-spin h-5 w-5 mr-3" viewBox="0 0 24 24"><circle class="opacity-25" cx="12" cy="12" r="10" stroke="currentColor" stroke-width="4" fill="none"></circle><path class="opacity-75" fill="currentColor" d="M4 12a8 8 0 018-8V0C5.373 0 0 5.373 0 12h4zm2 5.291A7.962 7.962 0 014 12H0c0 3.042 1.135 5.824 3 7.938l3-2.647z"></path></svg> Đang Mở...`;
        
        // Push 1 log lên Firebase báo hiệu Web ra lệnh
        await addDoc(collection(db, "logs"), {
            name: "Hùng Anh (Admin)",
            uid: "WEB_CONTROL",
            status: "Mo Cua Tu Xa",
            timestamp: serverTimestamp()
        });

        // Tạm thời UI cập nhật
        doorStatus.innerHTML = `Đang Mở 🔓`;
        doorStatus.className = "mt-2 text-2xl font-bold text-sky-400 flex items-center justify-center gap-2";

        // Tự động phục hồi UI sau 5s (thời gian Servo đóng lại)
        setTimeout(() => {
            doorStatus.innerHTML = `Đang Khóa 🔒`;
            doorStatus.className = "mt-2 text-2xl font-bold text-emerald-400 flex items-center justify-center gap-2";
            btnOpenDoor.disabled = false;
            btnOpenDoor.innerHTML = `<svg class="w-6 h-6" fill="none" stroke="currentColor" viewBox="0 0 24 24"><path stroke-linecap="round" stroke-linejoin="round" stroke-width="2" d="M8 11V7a4 4 0 118 0m-4 8v2m-6 4h12a2 2 0 002-2v-6a2 2 0 00-2-2H6a2 2 0 00-2 2v6a2 2 0 002 2z"></path></svg> MỞ CỬA TỪ XA`;
        }, 5000);

    } catch (e) {
        console.error("Lỗi khi mở cửa: ", e);
        alert("Có lỗi xảy ra khi truyền lệnh lên Firebase!");
        btnOpenDoor.disabled = false;
    }
});
