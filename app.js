// ==================== FIREBASE CONFIG ====================
const firebaseConfig = {
  apiKey: "AIzaSyBNE80VjvcwUd0VBXSZQQLLarI0SQHIQsQ",
  authDomain: "hung-anh-fn.firebaseapp.com",
  projectId: "hung-anh-fn",
  storageBucket: "hung-anh-fn.firebasestorage.app",
  messagingSenderId: "67209245744",
  appId: "1:67209245744:web:ee983db31e993a687aa9f5",
  measurementId: "G-0D0CKFGL3S"
};

firebase.initializeApp(firebaseConfig);
const db = firebase.firestore();
const auth = firebase.auth();
const analytics = firebase.analytics();

let currentUser = null; // logged-in Firebase user

// ==================== AUTH FUNCTIONS ====================
function switchAuthTab(tab) {
  document.getElementById('tab-login').classList.toggle('active', tab === 'login');
  document.getElementById('tab-register').classList.toggle('active', tab === 'register');
  document.getElementById('form-login').classList.toggle('active', tab === 'login');
  document.getElementById('form-register').classList.toggle('active', tab === 'register');
  // clear errors
  hideAuthError('login-error');
  hideAuthError('reg-error');
  document.getElementById('reg-success').classList.remove('show');
}

function showAuthError(id, msg) {
  const el = document.getElementById(id);
  el.textContent = msg;
  el.classList.add('show');
}

function hideAuthError(id) {
  document.getElementById(id).classList.remove('show');
}

function setAuthLoading(btnId, loading) {
  const btn = document.getElementById(btnId);
  btn.disabled = loading;
  if (btnId === 'btn-login') btn.textContent = loading ? 'Đang đăng nhập...' : 'Đăng nhập';
  if (btnId === 'btn-register') btn.textContent = loading ? 'Đang tạo tài khoản...' : 'Tạo tài khoản';
}

function loginUser() {
  const email = document.getElementById('login-email').value.trim();
  const password = document.getElementById('login-password').value;
  hideAuthError('login-error');

  if (!email) { showAuthError('login-error', '⚠️ Vui lòng nhập email'); return; }
  if (!password) { showAuthError('login-error', '⚠️ Vui lòng nhập mật khẩu'); return; }

  setAuthLoading('btn-login', true);
  auth.signInWithEmailAndPassword(email, password)
    .then(() => {
      // onAuthStateChanged will handle the rest
    })
    .catch(err => {
      setAuthLoading('btn-login', false);
      let msg = '❌ Đăng nhập thất bại. Vui lòng thử lại.';
      if (err.code === 'auth/user-not-found') msg = '❌ Tài khoản không tồn tại. Hãy đăng ký trước!';
      else if (err.code === 'auth/wrong-password' || err.code === 'auth/invalid-credential') msg = '❌ Mật khẩu không đúng. Vui lòng thử lại!';
      else if (err.code === 'auth/invalid-email') msg = '❌ Email không hợp lệ.';
      else if (err.code === 'auth/too-many-requests') msg = '⏳ Quá nhiều lần thử. Vui lòng chờ vài phút.';
      showAuthError('login-error', msg);
    });
}

function registerUser() {
  const name = document.getElementById('reg-name').value.trim();
  const email = document.getElementById('reg-email').value.trim();
  const password = document.getElementById('reg-password').value;
  const confirm = document.getElementById('reg-confirm').value;
  hideAuthError('reg-error');
  document.getElementById('reg-success').classList.remove('show');

  if (!name) { showAuthError('reg-error', '⚠️ Vui lòng nhập tên hiển thị'); return; }
  if (!email) { showAuthError('reg-error', '⚠️ Vui lòng nhập email'); return; }
  if (password.length < 6) { showAuthError('reg-error', '⚠️ Mật khẩu phải có ít nhất 6 ký tự'); return; }
  if (password !== confirm) { showAuthError('reg-error', '⚠️ Mật khẩu xác nhận không khớp'); return; }

  setAuthLoading('btn-register', true);
  auth.createUserWithEmailAndPassword(email, password)
    .then((userCredential) => {
      // Save display name and birthday to be used in loadUserData
      window._pendingRegName = name;
      window._pendingRegBirthday = document.getElementById('reg-birthday').value || '';
      const el = document.getElementById('reg-success');
      el.textContent = `✅ Chào mừng ${name}! Đang chuyển hướng...`;
      el.classList.add('show');
    })
    .catch(err => {
      setAuthLoading('btn-register', false);
      let msg = '❌ Đăng ký thất bại. Vui lòng thử lại.';
      if (err.code === 'auth/email-already-in-use') msg = '❌ Email này đã được đăng ký. Hãy đăng nhập!';
      else if (err.code === 'auth/invalid-email') msg = '❌ Email không hợp lệ.';
      else if (err.code === 'auth/weak-password') msg = '❌ Mật khẩu quá yếu. Dùng ít nhất 6 ký tự.';
      showAuthError('reg-error', msg);
    });
}


function logoutUser() {
  if (!confirm('Bạn có chắc muốn đăng xuất?')) return;
  auth.signOut().then(() => {
    document.getElementById('auth-screen').classList.remove('hidden');
    document.getElementById('main-app').style.display = 'none';
    // Reset form
    document.getElementById('login-email').value = '';
    document.getElementById('login-password').value = '';
    setAuthLoading('btn-login', false);
    switchAuthTab('login');
    showToast('Đã đăng xuất thành công 👋');
  });
}

// Auth state listener - gates app access
function initAuth() {
  auth.onAuthStateChanged(user => {
    if (user) {
      currentUser = user;
      document.getElementById('auth-screen').classList.add('hidden');
      document.getElementById('main-app').style.display = '';
      // Load user-specific data from Firestore
      loadUserData(user.uid);
    } else {
      currentUser = null;
      document.getElementById('auth-screen').classList.remove('hidden');
      document.getElementById('main-app').style.display = 'none';
    }
  });
}

// ==================== DATA LAYER ====================
const STORAGE_KEY = 'ha_finance_v2';

const DEFAULT_DATA = {
  profile: { name: 'Hùng Anh', email: 'hunganh@finance.vn' },
  wallets: [
    { id: 'w1', name: 'Ví tiền mặt', balance: 5000000, color: '#4F6BF0' },
    { id: 'w2', name: 'Tài khoản ngân hàng', balance: 12000000, color: '#00B37E' },
    { id: 'w3', name: 'Tiết kiệm', balance: 25000000, color: '#FF9800' },
  ],
  transactions: [
    { id: 't1', type: 'expense', category: 'food', note: 'Big C Supermarket', wallet: 'w1', amount: 145200, date: new Date().toISOString().slice(0,10) },
    { id: 't2', type: 'expense', category: 'utilities', note: 'Hóa đơn điện', wallet: 'w2', amount: 82000, date: getDateDaysAgo(1) },
    { id: 't3', type: 'expense', category: 'entertain', note: 'Netflix Subscription', wallet: 'w2', amount: 15990, date: getDateDaysAgo(14) },
    { id: 't4', type: 'income', category: 'salary', note: 'Lương tháng 3', wallet: 'w2', amount: 12000000, date: getDateDaysAgo(5) },
    { id: 't5', type: 'expense', category: 'transport', note: 'Đổ xăng', wallet: 'w1', amount: 250000, date: getDateDaysAgo(3) },
    { id: 't6', type: 'income', category: 'freelance', note: 'Dự án thiết kế web', wallet: 'w2', amount: 3500000, date: getDateDaysAgo(7) },
    { id: 't7', type: 'expense', category: 'health', note: 'Khám sức khỏe', wallet: 'w1', amount: 350000, date: getDateDaysAgo(10) },
    { id: 't8', type: 'expense', category: 'shop', note: 'Mua quần áo', wallet: 'w1', amount: 580000, date: getDateDaysAgo(12) },
  ],
  budgets: [
    { id: 'b1', category: 'food', limit: 3000000 },
    { id: 'b2', category: 'transport', limit: 800000 },
    { id: 'b3', category: 'entertain', limit: 500000 },
  ],
  settings: { darkMode: false, currency: '₫', notifyEnabled: true },
  notifications: [
    { text: 'Chi tiêu ăn uống đã đạt 85% ngân sách tháng này!', time: '2 giờ trước' },
    { text: 'Hóa đơn điện tháng 3 đã được ghi nhận.', time: 'Hôm qua' },
    { text: 'Bạn có thu nhập mới: ₫3,500,000 từ freelance.', time: '7 ngày trước' },
  ]
};

function getDateDaysAgo(days) {
  const d = new Date();
  d.setDate(d.getDate() - days);
  return d.toISOString().slice(0, 10);
}

let data = loadData();

function loadData() {
  try {
    const raw = localStorage.getItem(STORAGE_KEY);
    if (raw) return JSON.parse(raw);
  } catch(e) {}
  return JSON.parse(JSON.stringify(DEFAULT_DATA));
}

function saveData() {
  // Save to localStorage immediately (fast, offline-first)
  localStorage.setItem(STORAGE_KEY, JSON.stringify(data));
  // Sync to Firestore under user's own document
  if (currentUser) {
    db.collection('users').doc(currentUser.uid).collection('finance').doc('main').set(data)
      .catch(err => console.warn('⚠️ Firebase sync error:', err));
  }
}

// Load user-specific data from Firestore
function loadUserData(uid) {
  showSyncIndicator('Đang tải dữ liệu... ☁️');
  db.collection('users').doc(uid).collection('finance').doc('main').get()
    .then(doc => {
      if (doc.exists) {
        data = doc.data();
        localStorage.setItem(STORAGE_KEY, JSON.stringify(data));
      } else {
        // New user: seed with default data
        data = JSON.parse(JSON.stringify(DEFAULT_DATA));
        // Use the name entered during registration
        if (window._pendingRegName) {
          data.profile.name = window._pendingRegName;
          window._pendingRegName = null;
        }
        // Save birthday if provided
        if (window._pendingRegBirthday) {
          data.profile.birthday = window._pendingRegBirthday;
          window._pendingRegBirthday = null;
        }
        // Update profile email to their Firebase email
        if (currentUser && currentUser.email) {
          data.profile.email = currentUser.email;
        }
        db.collection('users').doc(uid).collection('finance').doc('main').set(data);
      }
      // Apply settings & render
      if (data.settings.darkMode) {
        document.body.classList.add('dark');
        document.getElementById('toggle-dark').checked = true;
      } else {
        document.body.classList.remove('dark');
        document.getElementById('toggle-dark').checked = false;
      }
      if (data.settings.notifyEnabled) document.getElementById('toggle-notify').checked = true;
      document.querySelector('.greeting').textContent = `Xin chào, ${data.profile.name}!`;
      document.querySelector('.settings-name').textContent = data.profile.name;
      document.querySelector('.settings-email').textContent = data.profile.email;
      renderDashboard();
      showSyncIndicator('Chào mừng trở lại! ✅');
      // 🎉 Check birthday & holidays
      setTimeout(() => checkGreetings(data.profile), 800);
    })
    .catch(err => {
      console.warn('⚠️ Không thể kết nối Firestore:', err);
      showSyncIndicator('Offline - dùng dữ liệu local 📱');
      renderDashboard();
    });
}

function showSyncIndicator(msg) {
  const toast = document.getElementById('toast');
  if (!toast) return;
  toast.textContent = msg;
  toast.classList.add('show');
  setTimeout(() => toast.classList.remove('show'), 3000);
}

// ==================== CATEGORIES ====================
const CATEGORIES = {
  food:      { label: 'Ăn uống',    emoji: '🛒', color: '#4CAF50' },
  transport: { label: 'Di chuyển',  emoji: '🚗', color: '#2196F3' },
  entertain: { label: 'Giải trí',   emoji: '🎬', color: '#E91E63' },
  health:    { label: 'Sức khỏe',   emoji: '💊', color: '#00BCD4' },
  shop:      { label: 'Mua sắm',    emoji: '🛍️', color: '#9C27B0' },
  utilities: { label: 'Tiện ích',   emoji: '⚡', color: '#FF9800' },
  education: { label: 'Học tập',    emoji: '📚', color: '#3F51B5' },
  other:     { label: 'Khác',       emoji: '💡', color: '#607D8B' },
  salary:    { label: 'Lương',      emoji: '💰', color: '#4CAF50' },
  freelance: { label: 'Freelance',  emoji: '💻', color: '#00B37E' },
  invest:    { label: 'Đầu tư',     emoji: '📈', color: '#FF5722' },
  gift:      { label: 'Quà tặng',   emoji: '🎁', color: '#E91E63' },
};
const EXPENSE_CATS = ['food','transport','entertain','health','shop','utilities','education','other'];
const INCOME_CATS  = ['salary','freelance','invest','gift','other'];
const WALLET_COLORS = ['#4F6BF0','#00B37E','#FF9800','#E91E63','#9C27B0','#00BCD4','#FF5722','#607D8B'];

// ==================== STATE ====================
let currentTxType = 'expense';
let selectedCategory = 'food';
let selectedWalletColor = WALLET_COLORS[0];
let txFilterMode = 'all';
let deleteMode = false;   // global transaction delete mode
let walletDeleteMode = false; // wallet delete mode
let budgetDeleteMode = false; // budget delete mode

// ==================== FORMATTERS ====================
function fmt(amount) {
  return data.settings.currency + new Intl.NumberFormat('vi-VN').format(Math.round(amount));
}

function fmtDate(dateStr) {
  const d = new Date(dateStr + 'T00:00:00');
  const today = new Date(); today.setHours(0,0,0,0);
  const yesterday = new Date(today); yesterday.setDate(today.getDate()-1);
  d.setHours(0,0,0,0);
  if (d.getTime() === today.getTime()) return 'Hôm nay';
  if (d.getTime() === yesterday.getTime()) return 'Hôm qua';
  return d.toLocaleDateString('vi-VN', {day:'2-digit', month:'2-digit', year:'numeric'});
}

// ==================== CALCULATIONS ====================
function getMonthTotals() {
  const now = new Date();
  const ym = `${now.getFullYear()}-${String(now.getMonth()+1).padStart(2,'0')}`;
  let income = 0, expense = 0;
  data.transactions.forEach(tx => {
    if (tx.date.startsWith(ym)) {
      if (tx.type === 'income') income += tx.amount;
      else if (tx.type === 'expense') expense += tx.amount;
    }
  });
  return { income, expense };
}

function getTotalBalance() {
  return data.wallets.reduce((s, w) => s + w.balance, 0);
}

function getSpendingByMonth(months) {
  const result = [];
  const labels = [];
  const now = new Date();
  for (let i = months-1; i >= 0; i--) {
    const d = new Date(now.getFullYear(), now.getMonth()-i, 1);
    const ym = `${d.getFullYear()}-${String(d.getMonth()+1).padStart(2,'0')}`;
    const total = data.transactions
      .filter(tx => tx.type === 'expense' && tx.date.startsWith(ym))
      .reduce((s, tx) => s + tx.amount, 0);
    result.push(total);
    labels.push(d.toLocaleDateString('vi-VN', {month:'short'}).toUpperCase());
  }
  return { data: result, labels };
}

function getBudgetSpending(category) {
  const now = new Date();
  const ym = `${now.getFullYear()}-${String(now.getMonth()+1).padStart(2,'0')}`;
  return data.transactions
    .filter(tx => tx.type === 'expense' && tx.category === category && tx.date.startsWith(ym))
    .reduce((s, tx) => s + tx.amount, 0);
}

// ==================== NAVIGATION ====================
function switchPage(pageId) {
  document.querySelectorAll('.page').forEach(p => p.classList.remove('active'));
  document.getElementById(pageId).classList.add('active');
  document.querySelectorAll('.nav-btn[data-page]').forEach(b => {
    b.classList.toggle('active', b.dataset.page === pageId);
  });
  if (pageId === 'page-home') renderDashboard();
  if (pageId === 'page-wallets') renderWallets();
  if (pageId === 'page-budgets') renderBudgets();
  if (pageId === 'page-transactions') renderAllTransactions();
}

// ==================== DASHBOARD ====================
function renderDashboard() {
  const { income, expense } = getMonthTotals();
  const totalBalance = getTotalBalance();

  document.getElementById('total-balance').textContent = fmt(totalBalance);
  document.getElementById('total-income').textContent = fmt(income);
  document.getElementById('total-expense').textContent = fmt(expense);

  const chartMonths = parseInt(document.getElementById('chart-period').value) || 6;
  document.getElementById('chart-total').innerHTML = `${fmt(expense)} <span class="chart-pct">-${((expense/(income||1))*100).toFixed(1)}%</span>`;

  renderChart(chartMonths);
  renderRecentTransactions();
}

function renderRecentTransactions() {
  const container = document.getElementById('recent-tx-list');
  const recent = [...data.transactions]
    .sort((a,b) => b.date.localeCompare(a.date))
    .slice(0, 5);
  container.innerHTML = recent.length ? recent.map(tx => txItemHTML(tx)).join('') : emptyState('Chưa có giao dịch nào');
  if (deleteMode) container.classList.add('delete-mode');
  else container.classList.remove('delete-mode');
  syncEditBtnLabel();
}

function txItemHTML(tx) {
  const cat = CATEGORIES[tx.category] || CATEGORIES.other;
  const sign = tx.type === 'income' ? '+' : '-';
  const cls = tx.type === 'income' ? 'income' : 'expense';
  return `
    <div class="tx-item" id="txitem-${tx.id}">
      <div class="tx-delete-btn" onclick="deleteTransaction('${tx.id}', event)" title="Xóa giao dịch">
        <svg viewBox="0 0 24 24"><path d="M6 19c0 1.1.9 2 2 2h8c1.1 0 2-.9 2-2V7H6v12zM19 4h-3.5l-1-1h-5l-1 1H5v2h14V4z"/></svg>
      </div>
      <div class="tx-icon" style="background:${cat.color}22">
        <span style="font-size:22px">${cat.emoji}</span>
      </div>
      <div class="tx-info">
        <div class="tx-name">${cat.label}</div>
        <div class="tx-sub">${tx.note || ''}</div>
      </div>
      <div class="tx-right">
        <div class="tx-amount ${cls}">${sign}${fmt(tx.amount)}</div>
        <div class="tx-date">${fmtDate(tx.date)}</div>
      </div>
    </div>
  `;
}

function emptyState(msg) {
  return `<div class="empty-state"><div class="empty-icon">📭</div><p>${msg}</p></div>`;
}

/* ---- Delete Mode ---- */
function toggleDeleteMode() {
  deleteMode = !deleteMode;
  // Apply/remove class on all tx lists
  ['recent-tx-list','all-tx-list'].forEach(listId => {
    const el = document.getElementById(listId);
    if (el) el.classList.toggle('delete-mode', deleteMode);
  });
  syncEditBtnLabel();
  if (deleteMode) showToast('Chế độ xóa bật — nhấn 🗑 để xóa giao dịch');
  else showToast('Đã thoát chế độ xóa');
}

function syncEditBtnLabel() {
  document.querySelectorAll('.btn-edit-mode').forEach(btn => {
    btn.textContent = deleteMode ? 'Xong' : 'Xóa';
    btn.classList.toggle('editing', deleteMode);
  });
}

function deleteTransaction(id, e) {
  if (e) e.stopPropagation();
  if (!deleteMode) return;   // only allow delete in delete mode
  const tx = data.transactions.find(t => t.id === id);
  if (!tx) return;
  const cat = CATEGORIES[tx.category] || CATEGORIES.other;
  const sign = tx.type === 'income' ? '+' : '-';
  // Animate item out
  const el = document.getElementById('txitem-' + id);
  if (el) {
    el.style.transition = 'all 0.3s ease';
    el.style.transform = 'translateX(-100%)';
    el.style.opacity = '0';
    el.style.maxHeight = el.offsetHeight + 'px';
    setTimeout(() => { el.style.maxHeight = '0'; el.style.marginBottom = '0'; el.style.padding = '0'; }, 280);
  }
  setTimeout(() => {
    const wallet = data.wallets.find(w => w.id === tx.wallet);
    if (wallet) {
      if (tx.type === 'income') wallet.balance -= tx.amount;
      else wallet.balance += tx.amount;
    }
    data.transactions = data.transactions.filter(t => t.id !== id);
    saveData();
    showToast(`Đã xóa: ${cat.emoji} ${sign}${fmt(tx.amount)}`);
    renderDashboard();
    renderAllTransactions();
    renderWallets();
    // Auto-exit delete mode if no transactions left
    if (data.transactions.length === 0 && deleteMode) {
      deleteMode = false;
      syncEditBtnLabel();
    }
  }, 320);
}

// ==================== CHART ====================
let chartInstance = null;

function renderChart(months) {
  const canvas = document.getElementById('spending-chart');
  const ctx = canvas.getContext('2d');
  const { data: chartData, labels } = getSpendingByMonth(months);

  // Set labels
  const labelsEl = document.getElementById('chart-labels');
  labelsEl.innerHTML = labels.map(l => `<span>${l}</span>`).join('');

  const W = canvas.parentElement.offsetWidth || 340;
  canvas.width = W * 2;
  canvas.height = 160 * 2;
  canvas.style.width = W + 'px';
  canvas.style.height = '160px';
  ctx.scale(2, 2);

  const w = W, h = 160;
  const paddingX = 10, paddingTop = 10, paddingBottom = 20;
  const plotW = w - paddingX * 2;
  const plotH = h - paddingTop - paddingBottom;

  ctx.clearRect(0, 0, w, h);

  const max = Math.max(...chartData, 1);
  const points = chartData.map((v, i) => ({
    x: paddingX + (i / (chartData.length - 1)) * plotW,
    y: paddingTop + plotH - (v / max) * plotH
  }));

  // Gradient fill
  const grad = ctx.createLinearGradient(0, paddingTop, 0, h);
  grad.addColorStop(0, 'rgba(79,107,240,0.3)');
  grad.addColorStop(1, 'rgba(79,107,240,0.02)');

  ctx.beginPath();
  ctx.moveTo(points[0].x, points[0].y);
  for (let i = 1; i < points.length; i++) {
    const cp1x = (points[i-1].x + points[i].x) / 2;
    ctx.bezierCurveTo(cp1x, points[i-1].y, cp1x, points[i].y, points[i].x, points[i].y);
  }
  ctx.lineTo(points[points.length-1].x, h - paddingBottom);
  ctx.lineTo(points[0].x, h - paddingBottom);
  ctx.closePath();
  ctx.fillStyle = grad;
  ctx.fill();

  // Line
  ctx.beginPath();
  ctx.moveTo(points[0].x, points[0].y);
  for (let i = 1; i < points.length; i++) {
    const cp1x = (points[i-1].x + points[i].x) / 2;
    ctx.bezierCurveTo(cp1x, points[i-1].y, cp1x, points[i].y, points[i].x, points[i].y);
  }
  ctx.strokeStyle = '#4F6BF0';
  ctx.lineWidth = 2.5;
  ctx.stroke();

  // Dots
  points.forEach(p => {
    ctx.beginPath();
    ctx.arc(p.x, p.y, 4, 0, Math.PI * 2);
    ctx.fillStyle = '#4F6BF0';
    ctx.fill();
    ctx.beginPath();
    ctx.arc(p.x, p.y, 2.5, 0, Math.PI * 2);
    ctx.fillStyle = '#fff';
    ctx.fill();
  });
}

// ==================== ALL TRANSACTIONS ====================
function renderAllTransactions() {
  const container = document.getElementById('all-tx-list');
  let txs = [...data.transactions].sort((a,b) => b.date.localeCompare(a.date));
  if (txFilterMode !== 'all') txs = txs.filter(tx => tx.type === txFilterMode);
  container.innerHTML = txs.length ? txs.map(tx => txItemHTML(tx)).join('') : emptyState('Không có giao dịch');
  if (deleteMode) container.classList.add('delete-mode');
  else container.classList.remove('delete-mode');
  syncEditBtnLabel();
}

// Filter tabs
document.querySelectorAll('.filter-tab').forEach(tab => {
  tab.addEventListener('click', () => {
    txFilterMode = tab.dataset.filter;
    document.querySelectorAll('.filter-tab').forEach(t => t.classList.remove('active'));
    tab.classList.add('active');
    renderAllTransactions();
  });
});

// ==================== WALLETS ====================
function toggleWalletDeleteMode() {
  walletDeleteMode = !walletDeleteMode;
  renderWallets();
  syncWalletEditBtn();
  if (walletDeleteMode) showToast('Chế độ xóa ví bật — nhấn 🗑 để xóa ví');
  else showToast('Đã thoát chế độ xóa ví');
}

function syncWalletEditBtn() {
  const btn = document.getElementById('btn-wallet-delete-mode');
  if (!btn) return;
  btn.textContent = walletDeleteMode ? 'Xong' : 'Xóa';
  btn.classList.toggle('editing', walletDeleteMode);
}

function deleteWallet(id, e) {
  if (e) e.stopPropagation();
  if (!walletDeleteMode) return;
  if (data.wallets.length <= 1) {
    showToast('Phải có ít nhất 1 ví tiền!');
    return;
  }
  const wallet = data.wallets.find(w => w.id === id);
  if (!wallet) return;
  // Animate out
  const el = document.getElementById('walletitem-' + id);
  if (el) {
    el.style.transition = 'all 0.3s ease';
    el.style.transform = 'translateX(-110%)';
    el.style.opacity = '0';
    el.style.maxHeight = el.offsetHeight + 'px';
    setTimeout(() => { el.style.maxHeight = '0'; el.style.marginBottom = '0'; }, 280);
  }
  setTimeout(() => {
    // Move orphan transactions to first remaining wallet
    const remaining = data.wallets.filter(w => w.id !== id);
    if (remaining.length > 0) {
      data.transactions.forEach(tx => {
        if (tx.wallet === id) tx.wallet = remaining[0].id;
      });
    }
    data.wallets = data.wallets.filter(w => w.id !== id);
    saveData();
    showToast(`Đã xóa ví "${wallet.name}"`);
    walletDeleteMode = data.wallets.length > 0 ? walletDeleteMode : false;
    renderWallets();
    renderDashboard();
    syncWalletEditBtn();
  }, 320);
}

function renderWallets() {
  const container = document.getElementById('wallets-list');
  container.innerHTML = data.wallets.map(w => `
    <div class="wallet-item" id="walletitem-${w.id}">
      <div class="wallet-delete-btn" onclick="deleteWallet('${w.id}', event)" title="Xóa ví">
        <svg viewBox="0 0 24 24"><path d="M6 19c0 1.1.9 2 2 2h8c1.1 0 2-.9 2-2V7H6v12zM19 4h-3.5l-1-1h-5l-1 1H5v2h14V4z"/></svg>
      </div>
      <div class="wallet-color-bar" style="background:${w.color}"></div>
      <div class="wallet-info">
        <div class="wallet-name">${w.name}</div>
        <div class="wallet-balance-text">Số dư hiện tại</div>
      </div>
      <div class="wallet-amount" style="color:${w.color}">${fmt(w.balance)}</div>
    </div>
  `).join('');
  if (walletDeleteMode) container.classList.add('wallet-delete-mode');
  else container.classList.remove('wallet-delete-mode');
  syncWalletEditBtn();
  renderWalletPie();
}

function renderWalletPie() {
  const canvas = document.getElementById('wallet-pie-chart');
  const ctx = canvas.getContext('2d');
  const W = canvas.parentElement.offsetWidth || 300;
  const size = Math.min(W, 200);
  canvas.width = size * 2; canvas.height = size * 2;
  canvas.style.width = size + 'px'; canvas.style.height = size + 'px';
  ctx.scale(2, 2);

  const cx = size/2, cy = size/2, r = size/2 - 16;
  const total = data.wallets.reduce((s, w) => s + w.balance, 0) || 1;
  let angle = -Math.PI/2;

  data.wallets.forEach(w => {
    const slice = (w.balance / total) * Math.PI * 2;
    ctx.beginPath();
    ctx.moveTo(cx, cy);
    ctx.arc(cx, cy, r, angle, angle + slice);
    ctx.closePath();
    ctx.fillStyle = w.color;
    ctx.fill();
    angle += slice;
  });

  // Center hole
  ctx.beginPath();
  ctx.arc(cx, cy, r * 0.55, 0, Math.PI * 2);
  ctx.fillStyle = document.body.classList.contains('dark') ? '#1A1D27' : '#fff';
  ctx.fill();

  // Legend
  document.getElementById('wallet-legend').innerHTML = data.wallets.map(w => `
    <div class="legend-item">
      <div class="legend-dot" style="background:${w.color}"></div>
      ${w.name}: ${fmt(w.balance)}
    </div>
  `).join('');
}

// ==================== BUDGETS ====================
function toggleBudgetDeleteMode() {
  budgetDeleteMode = !budgetDeleteMode;
  renderBudgets();
  if (budgetDeleteMode) showToast('Chế độ xóa ngân sách bật — nhấn 🗑 để xóa');
  else showToast('Đã thoát chế độ xóa ngân sách');
}

function syncBudgetEditBtn() {
  const btn = document.getElementById('btn-budget-delete-mode');
  if (!btn) return;
  btn.textContent = budgetDeleteMode ? 'Xong' : 'Xóa';
  btn.classList.toggle('editing', budgetDeleteMode);
}

function deleteBudget(id, e) {
  if (e) e.stopPropagation();
  if (!budgetDeleteMode) return;
  const budget = data.budgets.find(b => b.id === id);
  if (!budget) return;
  const cat = CATEGORIES[budget.category] || CATEGORIES.other;
  const el = document.getElementById('budgetitem-' + id);
  if (el) {
    el.style.transition = 'all 0.3s ease';
    el.style.transform = 'translateX(-110%)';
    el.style.opacity = '0';
    el.style.maxHeight = el.offsetHeight + 'px';
    setTimeout(() => { el.style.maxHeight = '0'; el.style.marginBottom = '0'; }, 280);
  }
  setTimeout(() => {
    data.budgets = data.budgets.filter(b => b.id !== id);
    saveData();
    showToast(`Đã xóa ngân sách: ${cat.emoji} ${cat.label}`);
    if (data.budgets.length === 0) budgetDeleteMode = false;
    renderBudgets();
  }, 320);
}

function renderBudgets() {
  const { expense } = getMonthTotals();
  const totalBudget = data.budgets.reduce((s, b) => s + b.limit, 0);
  const pct = totalBudget ? Math.min((expense / totalBudget) * 100, 100) : 0;

  document.getElementById('budget-total-display').textContent = fmt(totalBudget);
  const fill = document.getElementById('budget-bar-fill');
  fill.style.width = pct + '%';
  fill.className = 'budget-bar-fill' + (pct > 85 ? ' danger' : '');
  document.getElementById('budget-pct-text').textContent = `${pct.toFixed(0)}% đã dùng • Còn lại ${fmt(Math.max(totalBudget - expense, 0))}`;

  const container = document.getElementById('budgets-list');
  container.innerHTML = data.budgets.map(b => {
    const cat = CATEGORIES[b.category] || CATEGORIES.other;
    const spent = getBudgetSpending(b.category);
    const bpct = Math.min((spent / b.limit) * 100, 100);
    const color = bpct > 85 ? '#E63946' : bpct > 60 ? '#FF9800' : '#4F6BF0';
    return `
      <div class="budget-item" id="budgetitem-${b.id}">
        <div class="budget-delete-btn" onclick="deleteBudget('${b.id}', event)" title="Xóa ngân sách">
          <svg viewBox="0 0 24 24"><path d="M6 19c0 1.1.9 2 2 2h8c1.1 0 2-.9 2-2V7H6v12zM19 4h-3.5l-1-1h-5l-1 1H5v2h14V4z"/></svg>
        </div>
        <div class="budget-item-header">
          <div class="budget-cat">
            <span style="font-size:20px">${cat.emoji}</span>
            ${cat.label}
          </div>
          <div class="budget-amounts">${fmt(spent)} / ${fmt(b.limit)}</div>
        </div>
        <div class="budget-item-bar">
          <div class="budget-item-fill" style="width:${bpct}%;background:${color}"></div>
        </div>
      </div>
    `;
  }).join('') || emptyState('Chưa có ngân sách nào');

  if (budgetDeleteMode) container.classList.add('budget-delete-mode');
  else container.classList.remove('budget-delete-mode');
  syncBudgetEditBtn();
}

// ==================== ADD TRANSACTION ====================
function openAddModal() {
  setTxType('expense');
  document.getElementById('tx-amount').value = '';
  document.getElementById('tx-note').value = '';
  document.getElementById('tx-date').value = new Date().toISOString().slice(0,10);
  buildCategoryGrid();
  buildWalletSelect();
  openModal('modal-add');
}

function setTxType(type) {
  currentTxType = type;
  ['expense','income','transfer'].forEach(t => {
    document.getElementById('type-' + t).classList.toggle('active', t === type);
  });
  buildCategoryGrid();
}

function buildCategoryGrid() {
  const cats = currentTxType === 'income' ? INCOME_CATS : EXPENSE_CATS;
  selectedCategory = cats[0];
  document.getElementById('category-grid').innerHTML = cats.map(key => {
    const c = CATEGORIES[key];
    return `<button class="cat-btn ${key === selectedCategory ? 'selected':''}" onclick="selectCategory('${key}')">
      <span class="cat-emoji">${c.emoji}</span>${c.label}
    </button>`;
  }).join('');
}

function selectCategory(key) {
  selectedCategory = key;
  document.querySelectorAll('#category-grid .cat-btn').forEach(btn => btn.classList.remove('selected'));
  event.currentTarget.classList.add('selected');
}

function buildWalletSelect() {
  document.getElementById('tx-wallet').innerHTML = data.wallets.map(w =>
    `<option value="${w.id}">${w.name} (${fmt(w.balance)})</option>`
  ).join('');
}

function saveTransaction() {
  const amount = parseFloat(document.getElementById('tx-amount').value);
  if (!amount || amount <= 0) { showToast('Vui lòng nhập số tiền hợp lệ'); return; }
  const walletId = document.getElementById('tx-wallet').value;
  const wallet = data.wallets.find(w => w.id === walletId);
  if (!wallet) { showToast('Vui lòng chọn ví'); return; }

  if (currentTxType === 'expense' && wallet.balance < amount) {
    showToast('Số dư ví không đủ!'); return;
  }

  const tx = {
    id: 't' + Date.now(),
    type: currentTxType,
    category: selectedCategory,
    note: document.getElementById('tx-note').value || CATEGORIES[selectedCategory]?.label,
    wallet: walletId,
    amount,
    date: document.getElementById('tx-date').value || new Date().toISOString().slice(0,10)
  };

  if (tx.type === 'income') wallet.balance += amount;
  else if (tx.type === 'expense') wallet.balance -= amount;

  data.transactions.push(tx);
  saveData();
  closeModal('modal-add');
  showToast('Đã thêm giao dịch thành công! ✅');
  renderDashboard();
  renderWallets();
}

// ==================== ADD WALLET ====================
document.getElementById('btn-add-wallet').onclick = () => {
  document.getElementById('wallet-name').value = '';
  document.getElementById('wallet-balance').value = '';
  buildColorGrid();
  openModal('modal-wallet');
};

function buildColorGrid() {
  selectedWalletColor = WALLET_COLORS[0];
  document.getElementById('wallet-color-grid').innerHTML = WALLET_COLORS.map(c =>
    `<div class="color-dot ${c===selectedWalletColor?'selected':''}" style="background:${c}" onclick="selectWalletColor('${c}')"></div>`
  ).join('');
}

function selectWalletColor(color) {
  selectedWalletColor = color;
  document.querySelectorAll('#wallet-color-grid .color-dot').forEach(d => {
    d.classList.toggle('selected', d.style.background === color);
  });
}

function saveWallet() {
  const name = document.getElementById('wallet-name').value.trim();
  const balance = parseFloat(document.getElementById('wallet-balance').value) || 0;
  if (!name) { showToast('Vui lòng nhập tên ví'); return; }
  data.wallets.push({ id: 'w' + Date.now(), name, balance, color: selectedWalletColor });
  saveData();
  closeModal('modal-wallet');
  showToast('Đã tạo ví mới! 💼');
  renderWallets();
}

// ==================== ADD BUDGET ====================
document.getElementById('btn-add-budget').onclick = () => {
  const sel = document.getElementById('budget-category');
  sel.innerHTML = EXPENSE_CATS.map(key => {
    const c = CATEGORIES[key];
    return `<option value="${key}">${c.emoji} ${c.label}</option>`;
  }).join('');
  document.getElementById('budget-limit').value = '';
  openModal('modal-budget');
};

function saveBudget() {
  const category = document.getElementById('budget-category').value;
  const limit = parseFloat(document.getElementById('budget-limit').value);
  if (!limit || limit <= 0) { showToast('Vui lòng nhập giới hạn ngân sách'); return; }
  const existing = data.budgets.findIndex(b => b.category === category);
  if (existing >= 0) data.budgets[existing].limit = limit;
  else data.budgets.push({ id: 'b' + Date.now(), category, limit });
  saveData();
  closeModal('modal-budget');
  showToast('Đã lưu ngân sách! 📊');
  renderBudgets();
}

// ==================== SETTINGS ====================
const CURRENCIES = [
  { symbol: '₫',  code: 'VND', label: 'Việt Nam Đồng',   flag: '🆻🇳' },
  { symbol: '$',   code: 'USD', label: 'US Dollar',         flag: '🇺🇸' },
  { symbol: '€',  code: 'EUR', label: 'Euro',               flag: '🇪🇺' },
  { symbol: '¥',  code: 'JPY', label: 'Japanese Yen',       flag: '🇯🇵' },
  { symbol: '£',  code: 'GBP', label: 'British Pound',      flag: '🇬🇧' },
  { symbol: '¥',  code: 'CNY', label: 'Tệ NDT (Trung Quốc)', flag: '🇨🇳' },
  { symbol: '₩', code: 'KRW', label: 'Korean Won',          flag: '🇰🇷' },
  { symbol: '฿',  code: 'THB', label: 'Thai Baht',          flag: '🇹🇭' },
  { symbol: 'S$',  code: 'SGD', label: 'Singapore Dollar',   flag: '🇸🇬' },
  { symbol: 'A$',  code: 'AUD', label: 'Australian Dollar',  flag: '🇦🇺' },
  { symbol: 'C$',  code: 'CAD', label: 'Canadian Dollar',    flag: '🇨🇦' },
  { symbol: '₹',  code: 'INR', label: 'Indian Rupee',        flag: '🇮🇳' },
];

function openEditProfile() {
  document.getElementById('profile-name').value = data.profile.name || '';
  document.getElementById('profile-email').value = data.profile.email || '';
  document.getElementById('profile-birthday').value = data.profile.birthday || '';
  openModal('modal-profile');
}

function saveProfile() {
  const newName = document.getElementById('profile-name').value.trim();
  if (!newName) { showToast('Vui lòng nhập tên!'); return; }
  data.profile.name = newName;
  data.profile.email = document.getElementById('profile-email').value.trim();
  data.profile.birthday = document.getElementById('profile-birthday').value || '';
  saveData();
  document.querySelector('.settings-name').textContent = data.profile.name;
  document.querySelector('.settings-email').textContent = data.profile.email;
  document.querySelector('.greeting').textContent = `Xin chào, ${data.profile.name}!`;
  closeModal('modal-profile');
  showToast('Đã cập nhật hồ sơ! ✅');
}

function toggleDarkMode() {
  const chk = document.getElementById('toggle-dark');
  chk.checked = !chk.checked;
  data.settings.darkMode = chk.checked;
  document.body.classList.toggle('dark', chk.checked);
  saveData();
  setTimeout(renderWallets, 100);
}

document.getElementById('toggle-dark').addEventListener('change', function() {
  data.settings.darkMode = this.checked;
  document.body.classList.toggle('dark', this.checked);
  saveData();
  setTimeout(renderWallets, 100);
});

function openNotifySettings() {
  const container = document.getElementById('notify-list');
  container.innerHTML = data.notifications.map(n => `
    <div class="notify-item">
      <div class="notify-dot"></div>
      <div>
        <div class="notify-text">${n.text}</div>
        <div class="notify-time">${n.time}</div>
      </div>
    </div>
  `).join('') || '<div class="empty-state" style="padding:30px 0"><div class="empty-icon">🔔</div><p>Không có thông báo</p></div>';
}
function openCurrencySettings() {
  const grid = document.getElementById('currency-grid');
  grid.innerHTML = CURRENCIES.map(c => `
    <button onclick="selectCurrency('${c.symbol}','${c.code}','${c.label}')" style="
      padding:14px 10px; border-radius:14px;
      border:2px solid ${data.settings.currency === c.symbol ? '#4F6BF0' : 'rgba(255,255,255,0.1)'};
      background:${data.settings.currency === c.symbol ? 'rgba(79,107,240,0.15)' : 'rgba(255,255,255,0.04)'};
      color:#fff; cursor:pointer; text-align:center; font-family:'Inter',sans-serif;
      transition:all 0.2s; font-size:13px;
    ">
      <div style="font-size:22px">${c.flag}</div>
      <div style="font-size:18px;font-weight:700;margin:4px 0">${c.symbol}</div>
      <div style="font-size:11px;opacity:0.6;line-height:1.3">${c.code}</div>
    </button>
  `).join('');
  openModal('modal-currency');
}

function selectCurrency(symbol, code, label) {
  data.settings.currency = symbol;
  saveData();
  document.getElementById('currency-display').textContent = `${label} (${symbol})`;
  openCurrencySettings(); // re-render grid to highlight selected
  renderDashboard();
  renderWallets();
  renderBudgets();
  showToast(`Đã chọn tiền tệ: ${symbol} ${code} ✅`);
}



function clearData() {
  if (confirm('Bạn có chắc muốn xóa toàn bộ giao dịch? Hành động này không thể hoàn tác.')) {
    data.transactions = [];
    data.wallets.forEach(w => w.balance = 0);
    saveData();
    renderDashboard();
    renderAllTransactions();
    renderBudgets();
    showToast('Đã xóa toàn bộ dữ liệu');
  }
}

// ==================== NOTIFICATIONS ====================
document.getElementById('btn-notify').onclick = () => {
  openNotifySettings();
  openModal('modal-notify');
  document.getElementById('notify-badge').style.display = 'none';
};

document.getElementById('btn-search').onclick = () => {
  const q = prompt('Tìm kiếm giao dịch:');
  if (!q) return;
  switchPage('page-transactions');
  setTimeout(() => {
    const results = data.transactions.filter(tx =>
      (tx.note||'').toLowerCase().includes(q.toLowerCase()) ||
      (CATEGORIES[tx.category]?.label||'').toLowerCase().includes(q.toLowerCase())
    ).sort((a,b) => b.date.localeCompare(a.date));
    document.getElementById('all-tx-list').innerHTML = results.length
      ? results.map(tx => txItemHTML(tx)).join('')
      : emptyState(`Không tìm thấy "${q}"`);
  }, 100);
};

// ==================== MODAL HELPERS ====================
function openModal(id) { document.getElementById(id).classList.add('open'); }
function closeModal(id) { document.getElementById(id).classList.remove('open'); }

// ==================== TOAST ====================
function showToast(msg) {
  const t = document.getElementById('toast');
  t.textContent = msg;
  t.classList.add('show');
  setTimeout(() => t.classList.remove('show'), 2500);
}

// ==================== CHART PERIOD CHANGE ====================
document.getElementById('chart-period').addEventListener('change', () => renderDashboard());

// ==================== INIT ====================
function init() {
  // 🔐 Start Firebase Auth gate — blocks app until logged in
  initAuth();
}

window.addEventListener('load', init);
window.addEventListener('resize', () => {
  const homePage = document.getElementById('page-home');
  if (homePage && homePage.classList.contains('active')) {
    const months = parseInt(document.getElementById('chart-period').value) || 6;
    renderChart(months);
  }
});

// ==================== GREETING SYSTEM ====================
const HOLIDAYS = [
  { month: 1,  day: 1,  emoji: '🎆', title: 'Chúc Mừng Năm Mới!', msg: 'Một năm mới tràn đầy sức khỏe, hạnh phúc và thịnh vượng! Mọi ước mơ đều thành hiện thực nhé! 🌟' },
  { month: 1,  day: 29, emoji: '🧧', title: 'Chúc Mừng Tết Nguyên Đán!', msg: 'Vạn sự như ý, tài lộc dồi dào, sức khỏe dồi dào! Năm mới an khang thịnh vượng! 🎊' },
  { month: 1,  day: 30, emoji: '🧧', title: 'Tết Nguyên Đán!', msg: 'Chúc bạn năm mới nhiều niềm vui, may mắn và tài lộc! 🍊' },
  { month: 1,  day: 31, emoji: '🧧', title: 'Năm Mới Đã Tới!', msg: 'Những điều tốt đẹp nhất đang chờ bạn phía trước! Chúc một năm bình an! 🎴' },
  { month: 3,  day: 8,  emoji: '🌸', title: 'Quốc Tế Phụ Nữ 8/3!', msg: 'Chúc mừng ngày Quốc tế Phụ nữ! Bạn thật đặc biệt và tuyệt vời! 💐' },
  { month: 4,  day: 30, emoji: '🇻🇳', title: 'Ngày Giải Phóng 30/4!', msg: 'Chúc mừng ngày Giải phóng miền Nam thống nhất đất nước! Vui lòng hào! 🎉' },
  { month: 5,  day: 1,  emoji: '⚒️', title: 'Quốc Tế Lao Động 1/5!', msg: 'Chúc mừng ngày Quốc tế Lao động! Chúc bạn luôn thành công trong công việc! 💪' },
  { month: 6,  day: 1,  emoji: '🧒', title: 'Quốc Tế Thiếu Nhi 1/6!', msg: 'Chúc mừng ngày Quốc tế Thiếu nhi! Hãy giữ mãi trái tim trẻ thơ! 🎠' },
  { month: 9,  day: 2,  emoji: '🎌', title: 'Quốc Khánh 2/9!', msg: 'Chúc mừng ngày Quốc khánh Việt Nam! Tự hào là người Việt Nam! 🇻🇳' },
  { month: 10, day: 20, emoji: '🌹', title: 'Ngày Phụ Nữ Việt Nam 20/10!', msg: 'Chúc mừng ngày Phụ nữ Việt Nam! Luôn xinh đẹp, hạnh phúc và thành công! 💕' },
  { month: 11, day: 20, emoji: '🍎', title: 'Ngày Nhà Giáo Việt Nam 20/11!', msg: 'Chúc mừng ngày Nhà giáo Việt Nam! Tri ân những người thầy, người cô! 📚' },
  { month: 12, day: 25, emoji: '🎄', title: 'Giáng Sinh An Lành!', msg: 'Chúc Giáng Sinh vui vẻ và hạnh phúc bên gia đình và người thân yêu! 🎅' },
];

function checkGreetings(profile) {
  const now = new Date();
  const todayM = now.getMonth() + 1;
  const todayD = now.getDate();

  // Check birthday first (highest priority)
  if (profile.birthday) {
    const [, bMonth, bDay] = profile.birthday.split('-').map(Number);
    if (bMonth === todayM && bDay === todayD) {
      showGreeting(
        '🎂',
        `Sinh Nhật Vui Vẻ, ${profile.name}!`,
        `Chúc bạn một ngày sinh nhật thật đặc biệt, tràn đầy niềm vui và hạnh phúc! Tuổi mới thật nhiều sức khỏe, thành công và may mắn nhé! 🎁🎉`,
        true
      );
      return;
    }
  }

  // Check holidays
  const holiday = HOLIDAYS.find(h => h.month === todayM && h.day === todayD);
  if (holiday) {
    showGreeting(holiday.emoji, holiday.title, holiday.msg, false);
  }
}

function showGreeting(emoji, title, msg, isBirthday) {
  document.getElementById('greeting-emoji').textContent = emoji;
  document.getElementById('greeting-title').textContent = title;
  document.getElementById('greeting-msg').textContent = msg;

  // Confetti for birthdays and special days
  const wrap = document.getElementById('confetti-wrap');
  wrap.innerHTML = '';
  if (isBirthday) {
    const colors = ['#4F6BF0','#FF9800','#E91E63','#00B37E','#FFD700','#9C27B0'];
    for (let i = 0; i < 20; i++) {
      const dot = document.createElement('div');
      dot.className = 'confetti-dot';
      dot.style.cssText = `
        left: ${Math.random() * 100}%;
        background: ${colors[Math.floor(Math.random() * colors.length)]};
        animation-duration: ${1.5 + Math.random() * 2}s;
        animation-delay: ${Math.random() * 2}s;
        width: ${6 + Math.random() * 6}px;
        height: ${6 + Math.random() * 6}px;
        border-radius: ${Math.random() > 0.5 ? '50%' : '2px'};
      `;
      wrap.appendChild(dot);
    }
  }

  document.getElementById('greeting-popup').classList.add('show');
}

function closeGreeting() {
  document.getElementById('greeting-popup').classList.remove('show');
}
