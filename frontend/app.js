const PAYMENT_API = 'http://localhost:8082';
const ORDER_API = 'http://localhost:8081';

async function createAccount() {
    const userId = document.getElementById('regUserId').value;
    if (!userId) return alert("Введите User ID");

    try {
        const res = await fetch(`${PAYMENT_API}/account`, {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({ user_id: parseInt(userId) })
        });
        if (res.ok) alert("Счет успешно создан!");
        else alert("Ошибка: " + res.statusText);
    } catch (e) { alert("Ошибка сети: " + e); }
}

async function topUp() {
    const userId = document.getElementById('topupUserId').value;
    const amount = document.getElementById('topupAmount').value;
    if (!userId || !amount) return alert("Заполните поля");

    try {
        const res = await fetch(`${PAYMENT_API}/account/topup`, {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({ user_id: parseInt(userId), amount: parseInt(amount) })
        });
        if (res.ok) {
            alert("Баланс пополнен!");
            document.getElementById('balUserId').value = userId;
            getBalance();
        } else alert("Ошибка: " + res.statusText);
    } catch (e) { alert("Ошибка сети: " + e); }
}

async function getBalance() {
    const userId = document.getElementById('balUserId').value;
    if (!userId) return alert("Введите User ID");

    try {
        const res = await fetch(`${PAYMENT_API}/account/balance?user_id=${userId}`);
        if (res.ok) {
            const data = await res.json();
            document.getElementById('balanceResult').innerText = data.balance;
        } else {
            document.getElementById('balanceResult').innerText = "Не найден";
        }
    } catch (e) { alert("Ошибка сети: " + e); }
}

async function createOrder() {
    const userId = document.getElementById('orderUserId').value;
    const amount = document.getElementById('orderAmount').value;
    const desc = document.getElementById('orderDesc').value || "Товар";

    if (!userId || !amount) return alert("Заполните поля");

    try {
        const res = await fetch(`${ORDER_API}/orders`, {
            method: 'POST',
            headers: {'Content-Type': 'application/json'},
            body: JSON.stringify({
                user_id: parseInt(userId),
                amount: parseInt(amount),
                description: desc
            })
        });

        const data = await res.json();

        if (res.ok) {
            const resDiv = document.getElementById('orderResult');
            resDiv.style.display = 'block';

            document.getElementById('createdId').innerText = data.order_id;
            document.getElementById('createdStatus').className = "text-muted";
            document.getElementById('createdStatus').innerText = "Обработка...";

            document.getElementById('historyUserId').value = userId;

            setTimeout(() => checkCreatedOrderStatus(data.order_id), 1000);

        } else {
            alert("Ошибка создания заказа");
        }
    } catch (e) { alert("Ошибка сети: " + e); }
}

async function checkCreatedOrderStatus(orderId) {
    try {
        const res = await fetch(`${ORDER_API}/orders/${orderId}`);
        const order = await res.json();

        const statusSpan = document.getElementById('createdStatus');
        statusSpan.innerText = order.status;

        statusSpan.className = `status-${order.status}`;

        getUserOrders();

        const balId = document.getElementById('balUserId').value;
        if(balId == order.user_id) getBalance();

    } catch (e) {
        console.error(e);
    }
}

async function getUserOrders() {
    const userId = document.getElementById('historyUserId').value;
    if (!userId) return alert("Введите User ID");

    const tbody = document.getElementById('ordersTableBody');
    tbody.innerHTML = '<tr><td colspan="4" class="text-center">Загрузка...</td></tr>';

    try {
        const res = await fetch(`${ORDER_API}/orders/user/${userId}`);
        const orders = await res.json();

        tbody.innerHTML = '';
        if (orders.length === 0) {
            tbody.innerHTML = '<tr><td colspan="4" class="text-center">Нет заказов</td></tr>';
            return;
        }

        orders.sort((a, b) => b.id - a.id);

        orders.forEach(order => {
            const row = `
                <tr>
                    <td>#${order.id}</td>
                    <td>${order.description}</td>
                    <td>${order.amount}</td>
                    <td class="status-${order.status}">${order.status}</td>
                </tr>
            `;
            tbody.innerHTML += row;
        });
    } catch (e) {
        tbody.innerHTML = `<tr><td colspan="4" class="text-center text-danger">Ошибка: ${e}</td></tr>`;
    }
}