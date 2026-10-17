ShopEase Simple C++ Backend (2 Modules)

Module 1: Product & Inventory Management
- Add, view, search, update, delete products
- Sort by price using Bubble Sort
- Stock/price validation

Module 2: Customer & Authentication Management
- Customer registration and login/logout
- View own profile
- Admin can view/search all customers and count them
- Admin login and separate admin panel

Demo admin login:
Username: admin
Password: admin123

Run in VS Code terminal:
g++ -std=c++17 main.cpp -o ShopEase
.\ShopEase.exe

Important:
- This is a beginner console prototype, not yet connected to your HTML frontend or MySQL.
- Products and customers are kept in memory and are lost when the program exits.
- Passwords are plain text for demo purposes only. Never use real passwords.
- Customer can browse products from the main menu using option 4.
