#include <iostream>
#include <vector>
#include <string>
using namespace std;

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
    string password;
};

// Email check: '@' ho, uske baad '.' ho, space na ho
bool isValidEmail(string email) {
    int at = email.find('@');
    int dot = email.rfind('.');

    if (at == -1) return false;          // @ not found
    if (at == 0) return false;           // @ shoud not be at the beginning
    if (dot < at) return false;          // . shoud be after @
    if (dot == (int)email.size() - 1) return false;  // . last mein nahi
    if (email.find(' ') != string::npos) return false; // space nahi
    return true;
}

// Password check: kam se kam 6 characters
bool isValidPassword(string password) {
    return password.size() >= 6;
}

// Name mein kam se kam 1 letter ho (sirf number na ho)
bool isValidName(string name) {
    for (int i = 0; i < (int)name.size(); i++) {
        if (isalpha(name[i])) return true;
    }
    return false;
}

  //lowercase (search ke liye)
string toLower(string s) {
    for (int i = 0; i < (int)s.size(); i++) {
        s[i] = tolower(s[i]);
    }
    return s;
}

class ProductModule {
private:
    vector<Product> products;

public:
    int searchIndex(int id) const {
        for (int i = 0; i < (int)products.size(); i++) {
            if (products[i].id == id) return i;
        }
        return -1;
    }

    void addProduct() {
        Product p;
        cout << "Product ID: ";
        cin >> p.id;

        if (searchIndex(p.id) != -1) {
            cout << "This product ID already exists.\n";
            return;
        }

        cout << "Product name: ";
        cin >> p.name;
        cout << "Price: ";
        cin >> p.price;
        cout << "Stock quantity: ";
        cin >> p.stock;

        if (p.price < 0 || p.stock < 0 || p.id < 0) {
            cout << "Price, stock, and ID cannot be negative.\n";
            return;
        }

        products.push_back(p);
        cout << "Product added successfully.\n";
    }

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

    // ID se search
    void searchById() const {
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

    // NAME se search - partial match bhi chalega
    void searchByName() const {
        string key;
        cout << "Enter product name: ";
        cin >> key;
        key = toLower(key);

        bool found = false;
        for (int i = 0; i < (int)products.size(); i++) {
            string name = toLower(products[i].name);

            if (name.find(key) != string::npos) {   // key name ke andar hai?
                cout << "Found: ID " << products[i].id
                     << ", " << products[i].name
                     << ", Price: " << products[i].price
                     << ", Stock: " << products[i].stock << "\n";
                found = true;
            }
        }

        if (!found) cout << "No product found with that name.\n";
    }

    // Search menu: ID ya Name
    void searchProduct() const {
        int option;
        cout << "Search by: 1. ID  2. Name\nChoice: ";
        cin >> option;

        if (option == 1) searchById();
        else if (option == 2) searchByName();
        else cout << "Invalid choice.\n";
    }

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

    // Bubble sort by price
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

    // Guest ke liye Browse Products menu 
    void customerProducts() const {
        int choice;
        do {
            cout << "\n--- Browse Products ---\n";
            cout << "1. View products\n2. Search product\n0. Back\nChoice: ";
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

//  MODULE 2: CUSTOMERS 
class CustomerModule {
private:
    vector<Customer> customers;

public:
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

        if (c.id <= 0) {
            cout << "ID must be a positive number.\n";
            return;
        }
        if (searchIndex(c.id) != -1) {
            cout << "This customer ID already exists.\n";
            return;
        }

        
        cout << "Name (use _ instead of spaces): ";
        cin >> c.name;
        while (!isValidName(c.name)) {
            cout << "Name must have at least one letter. Try again: ";
            cin >> c.name;
        }

        
        cout << "Email (example: abc@gmail.com): ";
        cin >> c.email;
        while (!isValidEmail(c.email)) {
            cout << "Invalid email. Try again: ";
            cin >> c.email;
        }

        
        cout << "Password (minimum 6 characters): ";
        cin >> c.password;
        while (!isValidPassword(c.password)) {
            cout << "Password too short. Try again: ";
            cin >> c.password;
        }

        customers.push_back(c);
        cout << "Registration successful. Remember your ID and password.\n";
    }

    
    void customerLogin(ProductModule &products) {
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
        customerMenu(customers[index], products);
    }

    // AFTER Login View products aur Search product
    void customerMenu(const Customer &customer, ProductModule &products) {
        int choice;
        do {
            cout << "\n--- CUSTOMER PANEL ---\n";
            cout << "1. View my profile\n";
            cout << "2. View products\n";
            cout << "3. Search product\n";
            cout << "0. Logout\nChoice: ";
            cin >> choice;

            if (choice == 1) {
                cout << "ID: " << customer.id << "\nName: " << customer.name
                     << "\nEmail: " << customer.email << "\n";
            } else if (choice == 2) {
                products.showProducts();
            } else if (choice == 3) {
                products.searchProduct();
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

//  MAIN
int main() {
    ProductModule products;
    CustomerModule customers;
    int choice;

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
            customers.customerLogin(products);
        } else if (choice == 4) {
            products.customerProducts();
        } else if (choice != 0) {
            cout << "Invalid choice.\n";
        }
    } while (choice != 0);

    cout << "ShopEase closed.\n";
    return 0;
}
