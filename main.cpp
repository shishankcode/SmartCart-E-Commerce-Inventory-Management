#include <iostream>
#include <vector>
#include <string>
using namespace std;

// DATA STRUCTURES: Product and Customer
struct Product {
    int id;
    string name;
    double price;
    int stock;
};

struct Customer {
    int id;
    string name;
    string email;
    string password; // Demo only: do not use real passwords
};

// MODULE 1: PRODUCT AND INVENTORY MANAGEMENT
class ProductModule {
private:
    vector<Product> products;

public:
    // Add a new product
    void addProduct() {
        Product p;
        cout << "Product ID: ";
        cin >> p.id;

        if (searchIndex(p.id) != -1) {
            cout << "This product ID already exists.\n";
            return;
        }

        cout << "Product name (use _ instead of spaces): ";
        cin >> p.name;
        cout << "Price: ";
        cin >> p.price;
        cout << "Stock quantity: ";
        cin >> p.stock;

        if (p.price < 0 || p.stock < 0) {
            cout << "Price and stock cannot be negative.\n";
            return;
        }

        products.push_back(p);
        cout << "Product added successfully.\n";
    }

    // Display all products
    void showProducts() const {
        if (products.empty()) {
            cout << "No products available.\n";
            return;
        }

        cout << "\nID\tName\tPrice\tStock\n";
        for (int i = 0; i < (int)products.size(); i++) {
            cout << products[i].id << "\t" << products[i].name << "\t"
                 << products[i].price << "\t" << products[i].stock << "\n";
        }
    }

    // Linear search by product ID
    int searchIndex(int id) const {
        for (int i = 0; i < (int)products.size(); i++) {
            if (products[i].id == id) {
                return i;
            }
        }
        return -1;
    }

    void searchProduct() const {
        int id;
        cout << "Enter product ID: ";
        cin >> id;

        int index = searchIndex(id);
        if (index == -1) {
            cout << "Product not found.\n";
        } else {
            cout << "Found: " << products[index].name
                 << ", Price: " << products[index].price
                 << ", Stock: " << products[index].stock << "\n";
        }
    }

    // Update price and stock
    void updateProduct() {
        int id;
        cout << "Enter product ID to update: ";
        cin >> id;

        int index = searchIndex(id);
        if (index == -1) {
            cout << "Product not found.\n";
            return;
        }

        double newPrice;
        int newStock;
        cout << "New price: ";
        cin >> newPrice;
        cout << "New stock: ";
        cin >> newStock;

        if (newPrice < 0 || newStock < 0) {
            cout << "Invalid values. Nothing updated.\n";
            return;
        }

        products[index].price = newPrice;
        products[index].stock = newStock;
        cout << "Product updated.\n";
    }

    // Delete a product
    void deleteProduct() {
        int id;
        cout << "Enter product ID to delete: ";
        cin >> id;

        int index = searchIndex(id);
        if (index == -1) {
            cout << "Product not found.\n";
            return;
        }

        products.erase(products.begin() + index);
        cout << "Product deleted.\n";
    }

    // Simple Bubble Sort by price (low to high)
    void sortProductsByPrice() {
        for (int i = 0; i < (int)products.size(); i++) {
            for (int j = 0; j + 1 < (int)products.size() - i; j++) {
                if (products[j].price > products[j + 1].price) {
                    Product temp = products[j];
                    products[j] = products[j + 1];
                    products[j + 1] = temp;
                }
            }
        }

        cout << "Products sorted by price.\n";
        showProducts();
    }

    // Customer-facing product view and search
    void customerProducts() const {
        int choice;
        do {
            cout << "\n--- Browse Products ---\n";
            cout << "1. View products\n2. Search product by ID\n0. Back\nChoice: ";
            cin >> choice;

            if (choice == 1) showProducts();
            else if (choice == 2) searchProduct();
            else if (choice != 0) cout << "Invalid choice.\n";
        } while (choice != 0);
    }

    void adminMenu() {
        int choice;
        do {
            cout << "\n--- ADMIN: PRODUCT & INVENTORY ---\n";
            cout << "1. Add product\n2. View products\n3. Search product\n";
            cout << "4. Update price/stock\n5. Delete product\n6. Sort by price\n0. Back\nChoice: ";
            cin >> choice;

            if (choice == 1) addProduct();
            else if (choice == 2) showProducts();
            else if (choice == 3) searchProduct();
            else if (choice == 4) updateProduct();
            else if (choice == 5) deleteProduct();
            else if (choice == 6) sortProductsByPrice();
            else if (choice != 0) cout << "Invalid choice.\n";
        } while (choice != 0);
    }
};

// MODULE 2: CUSTOMER AND AUTHENTICATION MANAGEMENT
class CustomerModule {
private:
    vector<Customer> customers;

public:
    // Linear search by customer ID
    int searchIndex(int id) const {
        for (int i = 0; i < (int)customers.size(); i++) {
            if (customers[i].id == id) return i;
        }
        return -1;
    }

    void registerCustomer() {
        Customer c;
        cout << "Choose customer ID: ";
        cin >> c.id;

        if (searchIndex(c.id) != -1) {
            cout << "This customer ID already exists.\n";
            return;
        }

        cout << "Name (use _ instead of spaces): ";
        cin >> c.name;
        cout << "Email: ";
        cin >> c.email;
        cout << "Password (demo only): ";
        cin >> c.password;

        customers.push_back(c);
        cout << "Registration successful. Remember your ID and password.\n";
    }

    void customerLogin() {
        int id;
        string password;

        cout << "Customer ID: ";
        cin >> id;
        cout << "Password: ";
        cin >> password;

        int index = searchIndex(id);
        if (index == -1 || customers[index].password != password) {
            cout << "Wrong ID or password.\n";
            return;
        }

        cout << "\nWelcome, " << customers[index].name << "!\n";
        customerMenu(customers[index]);
    }

    void customerMenu(const Customer& customer) {
        int choice;
        do {
            cout << "\n--- CUSTOMER PANEL ---\n";
            cout << "1. View my profile\n2. Browse products\n0. Logout\nChoice: ";
            cin >> choice;

            if (choice == 1) {
                cout << "ID: " << customer.id << "\nName: " << customer.name
                     << "\nEmail: " << customer.email << "\n";
            } else if (choice == 2) {
                // Products are provided by the main program through the helper below.
                cout << "Use the main menu's customer product browsing option.\n";
            } else if (choice != 0) {
                cout << "Invalid choice.\n";
            }
        } while (choice != 0);

        cout << "Customer logged out.\n";
    }

    void adminCustomerMenu() const {
        int choice;
        do {
            cout << "\n--- ADMIN: CUSTOMER RECORDS ---\n";
            cout << "1. View all customers\n2. Search customer by ID\n";
            cout << "3. Total customer count\n0. Back\nChoice: ";
            cin >> choice;

            if (choice == 1) {
                if (customers.empty()) cout << "No customers registered.\n";
                for (int i = 0; i < (int)customers.size(); i++) {
                    cout << "ID: " << customers[i].id << ", Name: " << customers[i].name
                         << ", Email: " << customers[i].email << "\n";
                }
            } else if (choice == 2) {
                int id;
                cout << "Customer ID: ";
                cin >> id;
                int index = searchIndex(id);
                if (index == -1) cout << "Customer not found.\n";
                else cout << "Found: " << customers[index].name
                          << ", Email: " << customers[index].email << "\n";
            } else if (choice == 3) {
                cout << "Total customers: " << customers.size() << "\n";
            } else if (choice != 0) {
                cout << "Invalid choice.\n";
            }
        } while (choice != 0);
    }
};

int main() {
    ProductModule products;
    CustomerModule customers;
    int choice;

    // Demo admin credentials; change these for your presentation.
    const string adminUsername = "admin";
    const string adminPassword = "admin123";

    do {
        cout << "\n========== ShopEase ==========\n";
        cout << "1. Admin login\n";
        cout << "2. Customer registration\n";
        cout << "3. Customer login\n";
        cout << "4. Browse products (guest)\n";
        cout << "0. Exit\nChoice: ";
        cin >> choice;

        if (choice == 1) {
            string username, password;
            cout << "Admin username: ";
            cin >> username;
            cout << "Admin password: ";
            cin >> password;

            if (username == adminUsername && password == adminPassword) {
                cout << "Admin login successful.\n";
                int adminChoice;
                do {
                    cout << "\n--- ADMIN PANEL ---\n";
                    cout << "1. Product & inventory management\n";
                    cout << "2. Customer management\n";
                    cout << "0. Logout\nChoice: ";
                    cin >> adminChoice;

                    if (adminChoice == 1) products.adminMenu();
                    else if (adminChoice == 2) customers.adminCustomerMenu();
                    else if (adminChoice != 0) cout << "Invalid choice.\n";
                } while (adminChoice != 0);
                cout << "Admin logged out.\n";
            } else {
                cout << "Wrong admin username or password.\n";
            }
        } else if (choice == 2) {
            customers.registerCustomer();
        } else if (choice == 3) {
            customers.customerLogin();
        } else if (choice == 4) {
            products.customerProducts();
        } else if (choice != 0) {
            cout << "Invalid choice.\n";
        }
    } while (choice != 0);

    cout << "ShopEase closed.\n";
    return 0;
}
