const STORAGE_KEY = "shopease_frontend_demo";

const starterProducts = [
  { id: 101, name: "Studio Headphones", category: "Electronics", price: 2499, stock: 15, emoji: "🎧", tag: "BESTSELLER", image: "https://images.unsplash.com/photo-1505740420928-5e560c06d30e?auto=format&fit=crop&w=700&q=80" },
  { id: 102, name: "Everyday Sneakers", category: "Accessories", price: 3199, stock: 8, emoji: "👟", tag: "JUST IN", image: "https://images.unsplash.com/photo-1542291026-7eec264c27ff?auto=format&fit=crop&w=700&q=80" },
  { id: 103, name: "Minimal Desk Lamp", category: "Home", price: 1799, stock: 12, emoji: "💡", tag: "GOOD FIND", image: "https://images.unsplash.com/photo-1507473885765-e6ed057f782c?auto=format&fit=crop&w=700&q=80" },
  { id: 104, name: "Everyday Backpack", category: "Accessories", price: 1899, stock: 3, emoji: "🎒", tag: "LOW STOCK", image: "https://images.unsplash.com/photo-1553062407-98eeb64c6a62?auto=format&fit=crop&w=700&q=80" },
  { id: 105, name: "Wireless Mouse", category: "Electronics", price: 899, stock: 20, emoji: "🖱️", tag: "", image: "https://images.unsplash.com/photo-1527814050087-3793815479db?auto=format&fit=crop&w=700&q=80" },
  { id: 106, name: "Ceramic Coffee Set", category: "Home", price: 1299, stock: 9, emoji: "☕", tag: "GOOD FIND", image: "https://images.unsplash.com/photo-1514228742587-6b1558fcca3d?auto=format&fit=crop&w=700&q=80" },
  { id: 107, name: "Mechanical Keyboard", category: "Electronics", price: 2799, stock: 6, emoji: "⌨️", tag: "", image: "https://images.unsplash.com/photo-1587829741301-dc798b83add3?auto=format&fit=crop&w=700&q=80" },
  { id: 108, name: "Soft Throw Cushion", category: "Home", price: 999, stock: 14, emoji: "🛋️", tag: "COZY PICK", image: "https://images.unsplash.com/photo-1584100936595-c0654b55a2e2?auto=format&fit=crop&w=700&q=80" }
];

function loadState() {
  try {
    const saved = JSON.parse(localStorage.getItem(STORAGE_KEY));
    if (saved && Array.isArray(saved.products) &&
        Array.isArray(saved.cart) && Array.isArray(saved.orders)) {
      return saved;
    }
  } catch (error) {
    console.warn("Using fresh demo data.", error);
  }

  return {
    products: starterProducts.map(product => ({ ...product })),
    cart: [],
    orders: []
  };
}

let state = loadState();
let toastTimer;

const $ = id => document.getElementById(id);

function money(value) {
  return new Intl.NumberFormat("en-IN", {
    style: "currency",
    currency: "INR",
    maximumFractionDigits: 0
  }).format(Number(value) || 0);
}

function safe(value) {
  return String(value ?? "").replace(/[&<>"']/g, char => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;",
    '"': "&quot;", "'": "&#39;"
  })[char]);
}

function saveState() {
  localStorage.setItem(STORAGE_KEY, JSON.stringify(state));
}

function showToast(message) {
  const toast = $("toast");
  toast.textContent = message;
  toast.classList.add("show");
  clearTimeout(toastTimer);
  toastTimer = setTimeout(() => toast.classList.remove("show"), 2500);
}

function findProduct(id) {
  return state.products.find(product => Number(product.id) === Number(id));
}

function cartLines() {
  return state.cart
    .map(item => ({ ...item, product: findProduct(item.productId) }))
    .filter(item => item.product);
}

function subtotal() {
  return cartLines().reduce(
    (sum, item) => sum + item.product.price * item.quantity, 0
  );
}

function shippingCost() {
  return subtotal() === 0 || subtotal() >= 999 ? 0 : 79;
}

function renderProducts() {
  const query = $("searchInput").value.trim().toLowerCase();
  const category = $("categoryFilter").value;
  const sort = $("sortFilter").value;

  let products = state.products.filter(product => {
    const matchesCategory = category === "All" || product.category === category;
    const matchesSearch = `${product.name} ${product.category}`
      .toLowerCase().includes(query);
    return matchesCategory && matchesSearch;
  });

  if (sort === "low") products.sort((a, b) => a.price - b.price);
  if (sort === "high") products.sort((a, b) => b.price - a.price);
  if (sort === "name") products.sort((a, b) => a.name.localeCompare(b.name));

  $("resultsCount").textContent = `${products.length} lovely finds`;

  if (!products.length) {
    $("productGrid").innerHTML =
      '<div class="no-products">No products found. Try another search.</div>';
    return;
  }

  $("productGrid").innerHTML = products.map(product => {
    const soldOut = product.stock <= 0;
    const tag = product.tag || (product.stock <= 5 ? "LOW STOCK" : "");

    return `
      <article class="product-card">
        <div class="product-image">
          <img src="${safe(product.image)}"
               alt="${safe(product.name)}"
               onerror="this.style.display='none';this.nextElementSibling.style.display='grid'">
          <span class="product-emoji" style="display:none">${safe(product.emoji)}</span>
          ${tag ? `<span class="product-tag">${safe(tag)}</span>` : ""}
          <button class="add-btn" onclick="addToCart(${product.id})"
            ${soldOut ? "disabled" : ""}>
            ${soldOut ? "Out of stock" : "＋ Add to cart"}
          </button>
        </div>
        <div class="product-info">
          <span class="product-category">${safe(product.category)}</span>
          <h3 class="product-name">${safe(product.name)}</h3>
          <div class="product-details">
            <span class="product-price">${money(product.price)}</span>
            <span class="product-stock ${product.stock <= 5 ? "low" : ""}">
              ${soldOut ? "Sold out" : product.stock <= 5 ? `Only ${product.stock} left` : "In stock"}
            </span>
          </div>
        </div>
      </article>
    `;
  }).join("");
}

function addToCart(id) {
  const product = findProduct(id);
  if (!product || product.stock <= 0) {
    showToast("This product is out of stock.");
    return;
  }

  const existing = state.cart.find(item => Number(item.productId) === Number(id));

  if (existing) {
    if (existing.quantity >= product.stock) {
      showToast("No more stock available in this demo.");
      return;
    }
    existing.quantity++;
  } else {
    state.cart.push({ productId: Number(id), quantity: 1 });
  }

  saveState();
  renderCart();
  showToast(`${product.name} added to cart.`);
}

function changeQuantity(id, change) {
  const item = state.cart.find(line => Number(line.productId) === Number(id));
  const product = findProduct(id);
  if (!item || !product) return;

  const next = item.quantity + change;

  if (next <= 0) {
    state.cart = state.cart.filter(line => Number(line.productId) !== Number(id));
  } else if (next > product.stock) {
    showToast("Quantity exceeds demo stock.");
    return;
  } else {
    item.quantity = next;
  }

  saveState();
  renderCart();
}

function removeFromCart(id) {
  state.cart = state.cart.filter(item => Number(item.productId) !== Number(id));
  saveState();
  renderCart();
  showToast("Item removed from cart.");
}

function renderCart() {
  const lines = cartLines();
  const quantity = lines.reduce((sum, item) => sum + item.quantity, 0);
  const sub = subtotal();
  const shipping = shippingCost();

  $("cartCount").textContent = quantity;
  $("drawerCartCount").textContent = `(${quantity})`;
  $("subtotal").textContent = money(sub);
  $("shipping").textContent = shipping ? money(shipping) : "Free";
  $("total").textContent = money(sub + shipping);
  $("checkoutTotal").textContent = money(sub + shipping);

  $("emptyCart").classList.toggle("hidden", lines.length > 0);
  $("cartSummary").classList.toggle("hidden", lines.length === 0);

  $("cartItems").innerHTML = lines.map(item => `
    <div class="cart-item">
      <div class="cart-item-emoji">${safe(item.product.emoji)}</div>
      <div>
        <strong>${safe(item.product.name)}</strong>
        <small>${money(item.product.price)} each</small>
        <div class="qty-controls">
          <button onclick="changeQuantity(${item.productId}, -1)">−</button>
          <span>${item.quantity}</span>
          <button onclick="changeQuantity(${item.productId}, 1)"
            ${item.quantity >= item.product.stock ? "disabled" : ""}>＋</button>
        </div>
      </div>
      <button class="remove-btn" onclick="removeFromCart(${item.productId})">×</button>
    </div>
  `).join("");
}

function openCart() {
  $("cartDrawer").classList.add("open");
  $("overlay").classList.remove("hidden");
  renderCart();
}

function closeCart() {
  $("cartDrawer").classList.remove("open");
  $("overlay").classList.add("hidden");
}

function openCheckout() {
  if (!cartLines().length) {
    showToast("Add a product to your cart first.");
    return;
  }

  closeCart();
  $("checkoutTotal").textContent = money(subtotal() + shippingCost());
  $("checkoutModal").classList.remove("hidden");
}

function closeCheckout() {
  $("checkoutModal").classList.add("hidden");
}

$("checkoutForm").addEventListener("submit", event => {
  event.preventDefault();

  const lines = cartLines();
  if (!lines.length) {
    showToast("Your cart is empty.");
    return;
  }

  for (const item of lines) {
    const product = findProduct(item.productId);
    if (!product || product.stock < item.quantity) {
      showToast("Please check the available stock.");
      return;
    }
  }

  const sub = subtotal();
  const discount = sub >= 5000 ? Math.round(sub * 0.10) : 0;
  const shipping = shippingCost();

  const order = {
    id: "SE" + String(Date.now()).slice(-8),
    date: new Date().toLocaleString("en-IN"),
    customer: {
      name: $("customerName").value.trim(),
      phone: $("customerPhone").value.trim(),
      email: $("customerEmail").value.trim(),
      address: $("customerAddress").value.trim(),
      city: $("customerCity").value.trim(),
      pincode: $("customerPincode").value.trim()
    },
    items: lines.map(item => ({
      productId: item.product.id,
      name: item.product.name,
      price: item.product.price,
      quantity: item.quantity
    })),
    subtotal: sub,
    discount,
    shipping,
    total: Math.max(0, sub - discount + shipping),
    status: "Placed"
  };

  // Demo only. The C++ backend will own stock and order calculations later.
  lines.forEach(item => {
    findProduct(item.productId).stock -= item.quantity;
  });

  state.orders.push(order);
  state.cart = [];
  saveState();
  closeCheckout();
  $("checkoutForm").reset();
  renderAll();
  showPage("orders");
  showToast(`Demo order ${order.id} placed successfully!`);
});

function showPage(page) {
  const pages = {
    shop: "shopPage",
    orders: "ordersPage",
    admin: "adminPage"
  };

  Object.values(pages).forEach(id => $(id).classList.add("hidden"));
  $(pages[page]).classList.remove("hidden");

  document.querySelectorAll(".nav-btn").forEach(button => {
    button.classList.toggle(
      "active",
      button.textContent.trim().toLowerCase().replace("my orders", "orders") === page
    );
  });

  closeCart();
  if (page === "orders") renderOrders();
  if (page === "admin") renderAdmin();
  window.scrollTo({ top: 0, behavior: "smooth" });
}

function scrollToProducts() {
  $("catalog").scrollIntoView({ behavior: "smooth" });
}

function resetFilters() {
  $("searchInput").value = "";
  $("categoryFilter").value = "All";
  $("sortFilter").value = "featured";
  renderProducts();
}

function renderOrders() {
  if (!state.orders.length) {
    $("ordersList").innerHTML = `
      <div class="empty-message">
        <h3>Your story starts with a good find.</h3>
        <p>You haven't placed a demo order yet.</p>
        <button class="primary-btn" onclick="showPage('shop')">Explore the shop →</button>
      </div>`;
    return;
  }

  $("ordersList").innerHTML = [...state.orders].reverse().map(order => `
    <article class="order-card">
      <div>
        <h3>Order #${safe(order.id)}</h3>
        <p>Placed ${safe(order.date)} · ${safe(order.customer.name)}</p>
        <p>${order.items.map(item => `${safe(item.name)} × ${item.quantity}`).join(" · ")}</p>
        ${order.discount ? `<p>Demo discount: ${money(order.discount)}</p>` : ""}
        ${order.status !== "Cancelled" ? `<button onclick="cancelOrder('${safe(order.id)}')">Cancel order</button>` : ""}
      </div>
      <div>
        <strong>${money(order.total)}</strong><br>
        <span class="status">${safe(order.status)}</span>
      </div>
    </article>
  `).join("");
}

function cancelOrder(id) {
  const order = state.orders.find(item => String(item.id) === String(id));
  if (!order || order.status === "Cancelled") return;
  if (!confirm(`Cancel demo order ${order.id}?`)) return;

  order.items.forEach(item => {
    const product = findProduct(item.productId);
    if (product) product.stock += item.quantity;
  });

  order.status = "Cancelled";
  saveState();
  renderAll();
  showToast("Demo order cancelled.");
}

$("productForm").addEventListener("submit", event => {
  event.preventDefault();

  const editId = $("editId").value;
  const productData = {
    name: $("productName").value.trim(),
    price: Number($("productPrice").value),
    stock: Number($("productStock").value),
    category: $("productCategory").value,
    emoji: $("productEmoji").value.trim() || "✦",
    image: "",
    tag: "NEW FIND"
  };

  if (!productData.name || productData.price <= 0 ||
      !Number.isFinite(productData.price) ||
      !Number.isInteger(productData.stock) || productData.stock < 0) {
    showToast("Enter a valid product name, price and stock.");
    return;
  }

  if (editId) {
    const product = findProduct(editId);
    if (product) Object.assign(product, productData);
    showToast("Demo product updated.");
  } else {
    const nextId = Math.max(100, ...state.products.map(p => Number(p.id))) + 1;
    state.products.push({ id: nextId, ...productData });
    showToast("Demo product added.");
  }

  saveState();
  resetProductForm();
  renderAll();
});

function resetProductForm() {
  $("productForm").reset();
  $("editId").value = "";
  $("formTitle").textContent = "Add a product";
  $("saveProductBtn").textContent = "Add product ＋";
  $("cancelEditBtn").classList.add("hidden");
}

function editProduct(id) {
  const product = findProduct(id);
  if (!product) return;

  $("editId").value = product.id;
  $("productName").value = product.name;
  $("productPrice").value = product.price;
  $("productStock").value = product.stock;
  $("productCategory").value = product.category;
  $("productEmoji").value = product.emoji;

  $("formTitle").textContent = "Edit product";
  $("saveProductBtn").textContent = "Save changes ✓";
  $("cancelEditBtn").classList.remove("hidden");
  $("productForm").scrollIntoView({ behavior: "smooth", block: "start" });
}

function deleteProduct(id) {
  const product = findProduct(id);
  if (!product) return;
  if (!confirm(`Delete "${product.name}" from demo inventory?`)) return;

  state.products = state.products.filter(item => Number(item.id) !== Number(id));
  state.cart = state.cart.filter(item => Number(item.productId) !== Number(id));
  saveState();
  renderAll();
  showToast("Demo product deleted.");
}

function renderAdmin() {
  $("totalProducts").textContent = state.products.length;
  $("totalStock").textContent = state.products.reduce((sum, p) => sum + p.stock, 0);
  $("lowStock").textContent = state.products.filter(p => p.stock <= 5).length;
  $("totalOrders").textContent = state.orders.length;

  $("inventoryBody").innerHTML = state.products.map(product => `
    <tr>
      <td>${safe(product.emoji)} ${safe(product.name)}</td>
      <td>${money(product.price)}</td>
      <td><span class="stock-chip ${product.stock === 0 ? "out" : product.stock <= 5 ? "low" : ""}">
        ${product.stock === 0 ? "Out of stock" : product.stock}
      </span></td>
      <td>
        <button onclick="editProduct(${product.id})">Edit</button>
        <button onclick="deleteProduct(${product.id})">Delete</button>
      </td>
    </tr>
  `).join("");

  $("adminOrdersList").innerHTML = state.orders.length
    ? [...state.orders].reverse().slice(0, 5).map(order => `
        <div class="admin-order">
          <div><strong>#${safe(order.id)} · ${safe(order.customer.name)}</strong>
            <small>${safe(order.date)} · ${safe(order.status)}</small></div>
          <strong>${money(order.total)}</strong>
        </div>
      `).join("")
    : '<p class="form-note">No demo orders yet.</p>';
}

function renderAll() {
  renderProducts();
  renderCart();
  renderOrders();
  renderAdmin();
}

$("year").textContent = new Date().getFullYear();
renderAll();