#include <stdio.h>
#include <sql.h>
#include <sqlext.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <stdbool.h>
#include "structs.h"

void userLoop(SQLHDBC);
void addCustomer(SQLHDBC);
double getDoubleFromSQLNumericStruct(SQL_NUMERIC_STRUCT*);
void** bindAttrsCustomer(SQLHSTMT, size_t*);


double getDoubleFromSQLNumericStruct(SQL_NUMERIC_STRUCT* field) {
    long long int value = 0;
    for (int i = 0; i < sizeof(field->val); i++) {
        value += ((long long int)field->val[i]) << (8 * i);
    }
    double result = (double)value / pow(10, field->scale);
    if (field->sign == 0) { 
        result = -result;
    }
    return result;
}
SQLVARCHAR* readAndBindVarChar(SQLHSTMT hstmt, int idx, int buf_len) {
    char*input = NULL;
    size_t n = 0;

    getline(&input, &n, stdin);
    // remove the \n
    input[strlen(input)-1] = 0;
    // put it into proper type for sql
    SQLVARCHAR* field = malloc(strlen(input)+1);
    strcpy((char*)field, input);
    SQLLEN length = strlen((char*)field);
    SQLBindParameter(hstmt, idx, SQL_PARAM_INPUT, SQL_C_CHAR, 
                     SQL_VARCHAR, buf_len, 0, field, length, NULL);
    free(input);
    return field;
}
SQLVARCHAR* readAndBindTinyText(SQLHSTMT hstmt, int idx) {
    char* input = NULL;
    size_t n = 0;

    getline(&input, &n, stdin);
    // remove the \n
    input[strlen(input)-1] = 0;
    // put it into proper type for sql
    SQLVARCHAR* field = malloc(strlen(input)+1);
    strcpy((char*)field, input);
    SQLLEN length = strlen((char*)field);
    SQLBindParameter(hstmt, idx, SQL_PARAM_INPUT, SQL_C_CHAR, 
                     SQL_VARCHAR, 255, 0, field, length, NULL);
    free(input);
    return field;
}
SQL_NUMERIC_STRUCT* readAndBindDecimal(SQLHSTMT hstmt, int idx, int precision, int scale) {
    char* input = NULL;
    size_t n = 0;
    if(getline(&input, &n, stdin) < 0) {
        free(input);
        printf("Error reading line\n");
        exit(-1);
    }
    SQL_NUMERIC_STRUCT* field = malloc(sizeof(SQL_NUMERIC_STRUCT));;
    field->precision = precision;
    field->scale = 4;
    long long int scaled  = strtod(input, NULL) * pow(10, scale);
    field->sign = (scaled >= 0) ? 1 : 0;
    // since SQL records sign, make it always positive
    scaled = llabs(scaled);
    // convert int to byte array
    for(int i = 0; i < sizeof(field->val); i++) {
        // add lowest 8 bits of scaled into val
        field->val[i] = (SQLCHAR) (scaled & 0xFF);

        // shift those bits out of the way
        scaled >>= 8;
    }

    SQLBindParameter(hstmt, idx, SQL_PARAM_INPUT, SQL_C_NUMERIC, 
                     SQL_NUMERIC, precision, scale, field, 0, NULL);
    free(input);
    return field;
}
SQLINTEGER* readAndBindInteger(SQLHSTMT hstmt, int idx) {
    char* input = NULL;
    size_t n = 0;
    if(getline(&input, &n, stdin) < 0) {
        free(input);
        printf("Error reading line\n");
        exit(-1);
    }
    SQLINTEGER* field = malloc(sizeof(SQLINTEGER));;
    *field = atoi(input);

    SQLBindParameter(hstmt, idx, SQL_PARAM_INPUT, SQL_C_SLONG, 
                     SQL_INTEGER, 0, 0, field, 0, NULL);
    free(input);
    return field;

}

void addCustomer(SQLHDBC dbc) {
    // allocation and transaction start
    SQLHSTMT hstmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &hstmt);
    SQLExecDirect(hstmt, (SQLCHAR*)"BEGIN", SQL_NTS);

    SQLCHAR* insert = (SQLCHAR*)"INSERT INTO Customers(Company, FirstName, LastName, \
                      Email, BusinessPhone, Address) VALUES(?, ?, ?, ?, ?, ?)";
    SQLPrepare(hstmt, insert, SQL_NTS);

    // setting all the attributes
    size_t num_attr_vals = 0;
    void** return_values = bindAttrsCustomer(hstmt, &num_attr_vals);

    SQLRETURN ret = SQLExecute(hstmt);
    if(SQL_SUCCEEDED(ret)) {
        printf("Comitting Change\n");
        SQLExecDirect(hstmt, (SQLCHAR*)"COMMIT", SQL_NTS);
    }
    else {
        printf("Rolling Back\n");
        SQLExecDirect(hstmt, (SQLCHAR*)"ROLLBACK", SQL_NTS);
    }
    SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
    for(int i = 0; i < num_attr_vals; i++) {
        free(return_values[i]);
    }
    free(return_values);
}
SQL_TIMESTAMP_STRUCT getSQLNow(SQLHDBC dbc) {
    SQLHSTMT hstmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &hstmt);
    SQL_TIMESTAMP_STRUCT date_time;
    SQLExecDirect(hstmt, (SQLCHAR*)"SELECT Now()", SQL_NTS);
    SQLBindCol(hstmt, 1, SQL_C_TYPE_TIMESTAMP, &date_time, sizeof(date_time), NULL);
    SQLFetch(hstmt);
    SQLFreeHandle(SQL_HANDLE_STMT, hstmt);

    return date_time;
}
// always assume num_attr_vals will be 0 when passed in 
void** bindAttrsCustomer(SQLHSTMT hstmt, size_t* num_attr_vals) {
    void** return_values = malloc(sizeof(void*) * *(num_attr_vals));

    printf("Enter the company name: ");
    SQLVARCHAR* company = readAndBindVarChar(hstmt, 1, 50);
    return_values = realloc(return_values, ++(*num_attr_vals) * sizeof(void*));
    return_values[(*num_attr_vals)-1] = company;

    printf("Enter the First Name: ");
    SQLVARCHAR* first_name = readAndBindVarChar(hstmt, 2, 50);
    return_values = realloc(return_values, ++(*num_attr_vals) * sizeof(void*));
    return_values[(*num_attr_vals)-1] = first_name;

    printf("Enter the Last Name: ");
    SQLVARCHAR* last_name = readAndBindVarChar(hstmt, 3, 50);
    return_values = realloc(return_values, ++(*num_attr_vals) * sizeof(void*));
    return_values[(*num_attr_vals)-1] = last_name;

    printf("Enter the Email: ");
    SQLVARCHAR* email = readAndBindVarChar(hstmt, 4, 50);
    return_values = realloc(return_values, ++(*num_attr_vals) * sizeof(void*));
    return_values[(*num_attr_vals)-1] = email;

    printf("Enter the Business Phone: ");
    SQLVARCHAR* phone = readAndBindVarChar(hstmt, 5, 25);
    return_values = realloc(return_values, ++(*num_attr_vals) * sizeof(void*));
    return_values[(*num_attr_vals)-1] = phone;

    printf("Enter the Address: ");
    SQLVARCHAR* address = readAndBindTinyText(hstmt, 6);
    return_values = realloc(return_values, ++(*num_attr_vals) * sizeof(void*));
    return_values[(*num_attr_vals)-1] = address;
    return return_values;
}
void addOrder(SQLHDBC dbc) {
    // for error messages
    SQLCHAR sqlState[6], errorMsg[SQL_MAX_MESSAGE_LENGTH];
    SQLINTEGER nativeError;
    SQLSMALLINT textLength;

    // handle setup and starting transaction
    SQLHSTMT hstmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &hstmt);
    SQLExecDirect(hstmt, (SQLCHAR*)"BEGIN", SQL_NTS);

    /* initial insert into Orders */
    SQLCHAR* insert = (SQLCHAR*)"INSERT INTO Orders(EmployeeID, CustomerID, ShipperID, OrderDate, TaxStatus, StatusID, ShipAddress) VALUES(?, ?, ?, ?, ?, ?, ?)";
    SQLPrepare(hstmt, insert, SQL_NTS);

    size_t number_attr_values = 0;
    void** return_values = malloc(sizeof(void*) * number_attr_values);
    // binding the attributes
    printf("Enter the EmployeeID: ");
    SQLINTEGER* employee_id = readAndBindInteger(hstmt, 1);
    return_values = realloc(return_values, ++number_attr_values * sizeof(void*));
    return_values[number_attr_values-1] = employee_id;

    printf("Enter the CustomerID: ");
    SQLINTEGER* customer_id = readAndBindInteger(hstmt, 2);
    return_values = realloc(return_values, ++number_attr_values * sizeof(void*));
    return_values[number_attr_values-1] = customer_id;

    printf("Enter the ShipperID: ");
    SQLINTEGER* shipper_id = readAndBindInteger(hstmt, 3);
    return_values = realloc(return_values, ++number_attr_values * sizeof(void*));
    return_values[number_attr_values-1] = shipper_id;

    // The date is found automatically
    SQL_TIMESTAMP_STRUCT order_date = getSQLNow(dbc);
    SQLBindParameter(hstmt, 4, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP, 
                     SQL_TYPE_TIMESTAMP, 0, 0, &order_date, 0, NULL);

    printf("Enter the TaxStatus: ");
    SQLINTEGER* tax_status = readAndBindInteger(hstmt, 5);
    return_values = realloc(return_values, ++number_attr_values * sizeof(void*));
    return_values[number_attr_values-1] = tax_status;

    // status Id will always be 0 = 'New'
    SQLINTEGER status_id = 0;
    SQLBindParameter(hstmt, 6, SQL_PARAM_INPUT, SQL_C_SLONG, 
                     SQL_INTEGER, 0, 0, &status_id, 0, NULL);
    printf("Enter the ShipAddress: ");
    SQLVARCHAR* ship_addr = readAndBindTinyText(hstmt, 7);
    return_values = realloc(return_values, ++number_attr_values * sizeof(void*));
    return_values[number_attr_values-1] = ship_addr;

    SQLRETURN ret = SQLExecute(hstmt);
    if(!SQL_SUCCEEDED(ret)) {
        printf("Rolling Back\n");
        SQLExecDirect(hstmt, (SQLCHAR*)"ROLLBACK", SQL_NTS);
        // freeing the values that were created in other functions
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
        for(int i = 0; i < number_attr_values; i++) {
            free(return_values[i]);
        }
        free(return_values);
        return;
    }
    /* end initial insert into Orders */

    // getting the ID of the order
    SQLINTEGER oid;
    SQLHSTMT hstmt2;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &hstmt2);
    SQLExecDirect(hstmt2, (SQLCHAR*)"SELECT LAST_INSERT_ID()", SQL_NTS);
    SQLBindCol(hstmt2, 1, SQL_INTEGER, &oid, sizeof(oid), NULL);
    SQLFetch(hstmt2);
    printf("OrderID just inserted is: %d\n", oid);

    // repeat for how many products they want to add in the order
    char* input = NULL;
    size_t n = 0;
    while(1) {
        // ask the user for as many products as they would like
        printf("Would you like to add a product to the order? Y/N: ");
        getline(&input, &n, stdin);
        // break if they decide they are done.
        if(strcasestr(input, "N")) break;

        SQLFreeStmt(hstmt, SQL_CLOSE);
        insert = (SQLCHAR*)"INSERT INTO Order_Details(OrderID, ProductID, Quantity, \
                 UnitPrice, Discount, StatusID) VALUES(?, ?, ?, ?, ?, ?)";
        SQLPrepare(hstmt, insert, SQL_NTS);
        // Binding orderID
        SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, 
                         SQL_INTEGER, 0, 0, &oid, 0, NULL);

        // TODO ask for product ID
        printf("Enter the productID: ");
        SQLINTEGER* product_id = readAndBindInteger(hstmt, 2);
        return_values = realloc(return_values, ++number_attr_values * sizeof(void*));
        return_values[number_attr_values-1] = product_id;

        // if the product is discontinued do not add it to the order
        SQLSMALLINT discontinued = 0;
        SQLFreeStmt(hstmt2, SQL_CLOSE);
        SQLCHAR* insert2 = "SELECT Discontinued FROM Products WHERE ID=?";
        SQLPrepare(hstmt2, insert2, SQL_NTS);
        SQLBindParameter(hstmt2, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 
                         0, 0, product_id, 0, NULL);
        SQLBindCol(hstmt2, 1, SQL_C_TINYINT, &discontinued, 0, NULL);
        SQLExecute(hstmt2);
        SQLFetch(hstmt2);
        if(discontinued) {
            printf("That product is discontinued canceling request for product.\n");
            continue;
        }
        printf("Enter the Order Quantity: ");
        SQL_NUMERIC_STRUCT* order_quantity = readAndBindDecimal(hstmt, 3, 16, 4);
        return_values = realloc(return_values, ++number_attr_values * sizeof(void*));
        return_values[number_attr_values-1] = order_quantity;

        printf("Enter the Unit Price: ");
        SQL_NUMERIC_STRUCT* unit_price = readAndBindDecimal(hstmt, 4, 16, 4);
        return_values = realloc(return_values, ++number_attr_values * sizeof(void*));
        return_values[number_attr_values-1] = unit_price;

        printf("Enter the Discount: ");
        SQL_NUMERIC_STRUCT* discount = readAndBindDecimal(hstmt, 5, 16, 4);
        return_values = realloc(return_values, ++number_attr_values * sizeof(void*));
        return_values[number_attr_values-1] = discount;

        // Bind the statusID
        SQLINTEGER status_id_details = 0;
        SQLBindParameter(hstmt, 6, SQL_PARAM_INPUT, SQL_C_SLONG,
                         SQL_INTEGER, 0, 0, &status_id_details, 0, NULL);
        ret = SQLExecute(hstmt);
        if(!SQL_SUCCEEDED(ret)) {
            SQLGetDiagRec(SQL_HANDLE_STMT, hstmt, 1, sqlState, &nativeError, errorMsg,
                          sizeof(errorMsg), &textLength);
            fprintf(stderr, "Error %s: %s\n", sqlState, errorMsg);
            printf("Rolling Back\n");
            SQLExecDirect(hstmt, (SQLCHAR*)"ROLLBACK", SQL_NTS);
            // cleanup and return early
            SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
            SQLFreeHandle(SQL_HANDLE_STMT, hstmt2);
            for(int i = 0; i < number_attr_values; i++) {
                free(return_values[i]);
            }
            free(return_values);
            free(input);
            return;
        }
    }
    printf("Commiting changes\n");
    SQLExecDirect(hstmt, (SQLCHAR*)"COMMIT", SQL_NTS);
    SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
    for(int i = 0; i < number_attr_values; i++) {
        free(return_values[i]);
    }
    free(return_values);
    free(input);
}
void deleteOrder(SQLHDBC dbc) {
    // handle setup and starting transaction
    SQLHSTMT hstmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &hstmt);
    SQLExecDirect(hstmt, (SQLCHAR*)"BEGIN", SQL_NTS);

    SQLCHAR* insert = (SQLCHAR*)"DELETE FROM Order_Details WHERE OrderID=?";
    SQLPrepare(hstmt, insert, SQL_NTS);
    printf("Enter the orderID to be deleted: ");
    SQLINTEGER* order_id = readAndBindInteger(hstmt, 1);
    SQLRETURN ret = SQLExecute(hstmt);
    if(!SQL_SUCCEEDED(ret)) {
        printf("Rolling Back\n");
        SQLExecDirect(hstmt, (SQLCHAR*)"ROLLBACK", SQL_NTS);
        // cleanup and return early
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
        free(order_id);
        return;
    }
    SQLFreeStmt(hstmt, SQL_CLOSE);
    insert = (SQLCHAR*)"DELETE FROM Orders WHERE OrderID=?";
    SQLPrepare(hstmt, insert, SQL_NTS);
    SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, order_id, 0, NULL);
    ret = SQLExecute(hstmt);
    if(!SQL_SUCCEEDED(ret)) {
        printf("Rolling Back\n");
        SQLExecDirect(hstmt, (SQLCHAR*)"ROLLBACK", SQL_NTS);
        // cleanup and return early
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
        free(order_id);
        return;
    }
    SQLExecDirect(hstmt, (SQLCHAR*)"COMMIT", SQL_NTS);
    SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
    free(order_id);
}
PRODUCT_INFO** read_products(SQLHDBC dbc) {
    size_t total_products = 0;
    PRODUCT_INFO** products_available = malloc(sizeof(PRODUCT_INFO) * total_products);
    // get information about every productID
    SQLCHAR* insert = (SQLCHAR*)"SELECT TransactionID, ProductID, Quantity, TransactionType FROM Inventory_Transactions";
    SQLExecDirect(hstmt, insert, SQL_NTS);
    SQLINTEGER tid;
    SQLINTEGER pid;
    SQLINTEGER quantity;
    SQLINTEGER transaction_type;
    SQLBindCol(hstmt, 1, SQL_INTEGER, &tid, sizeof(tid), NULL);
    SQLBindCol(hstmt, 2, SQL_INTEGER, &pid, sizeof(pid), NULL);
    SQLBindCol(hstmt, 3, SQL_INTEGER, &quantity, sizeof(quantity), NULL);
    SQLBindCol(hstmt, 4, SQL_INTEGER, &transaction_type, sizeof(transaction_type), NULL);
    char* input = NULL;
    size_t n = 0;
    while(SQL_SUCCEEDED(SQLFetch(hstmt))) {
        // figure out if this PID has been read already
        bool new = 1;
        for(int i = 0; i < total_products; i++) {
            if(products_available[i]->pid == pid) {
                new = 0;
                if(transaction_type == 1) {
                    products_available[i]->quantity_avail += quantity;
                }
                if(transaction_type == 2 || transaction_type == 3) {
                    products_available[i]->quantity_avail -= quantity;
                }
            }
        }
        if(new) {
            products_available = realloc(products_available, sizeof(PRODUCT_INFO) * ++total_products);
            products_available[total_products-1] = malloc(sizeof(PRODUCT_INFO));
            products_available[total_products-1]->pid = pid;
            // this is a purchase transaction
            if(transaction_type == 1) {
                products_available[total_products-1]->quantity_avail = quantity;
            }
            // this is a sold or on hold
            else if(transaction_type == 2 || transaction_type == 3) {
                products_available[total_products-1]->quantity_avail = 0 - quantity;
            }
        }
    }
}

void shipOrder(SQLHDBC dbc) {
    // handle setup and starting transaction
    SQLHSTMT hstmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &hstmt);
    SQLExecDirect(hstmt, (SQLCHAR*)"BEGIN", SQL_NTS);
    

    size_t total_products = 0;
    PRODUCT_INFO** products_available = malloc(sizeof(PRODUCT_INFO) * total_products);
    // get information about every productID
    SQLCHAR* insert = (SQLCHAR*)"SELECT TransactionID, ProductID, Quantity, TransactionType FROM Inventory_Transactions";
    SQLExecDirect(hstmt, insert, SQL_NTS);
    SQLINTEGER tid;
    SQLINTEGER pid;
    SQLINTEGER quantity;
    SQLINTEGER transaction_type;
    SQLBindCol(hstmt, 1, SQL_INTEGER, &tid, sizeof(tid), NULL);
    SQLBindCol(hstmt, 2, SQL_INTEGER, &pid, sizeof(pid), NULL);
    SQLBindCol(hstmt, 3, SQL_INTEGER, &quantity, sizeof(quantity), NULL);
    SQLBindCol(hstmt, 4, SQL_INTEGER, &transaction_type, sizeof(transaction_type), NULL);
    char* input = NULL;
    size_t n = 0;
    while(SQL_SUCCEEDED(SQLFetch(hstmt))) {
        // figure out if this PID has been read already
        bool new = 1;
        for(int i = 0; i < total_products; i++) {
            if(products_available[i]->pid == pid) {
                new = 0;
                if(transaction_type == 1) {
                    products_available[i]->quantity_avail += quantity;
                }
                if(transaction_type == 2 || transaction_type == 3) {
                    products_available[i]->quantity_avail -= quantity;
                }
            }
        }
        if(new) {
            products_available = realloc(products_available, sizeof(PRODUCT_INFO) * ++total_products);
            products_available[total_products-1] = malloc(sizeof(PRODUCT_INFO));
            products_available[total_products-1]->pid = pid;
            // this is a purchase transaction
            if(transaction_type == 1) {
                products_available[total_products-1]->quantity_avail = quantity;
            }
            // this is a sold or on hold
            else if(transaction_type == 2 || transaction_type == 3) {
                products_available[total_products-1]->quantity_avail = 0 - quantity;
            }
        }
    }

    // TODO ask for orderID to be shipped
    printf("Enter orderID to be shipped: ");
    getline(&input, &n, stdin);
    SQLINTEGER oid = atoi(input);
    // before doing anything make sure this id actually exists
    SQLFreeStmt(hstmt, SQL_CLOSE);
    insert = (SQLCHAR*)"SELECT COUNT(*) FROM Orders WHERE OrderID=?";
    SQLPrepare(hstmt, insert, SQL_NTS);
    SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &oid, 0, NULL);
    SQLINTEGER check=0;
    SQLBindCol(hstmt, 1, SQL_INTEGER, &check, sizeof(check), NULL);
    SQLRETURN ret = SQLExecute(hstmt);
    if(!SQL_SUCCEEDED(ret)) {
        SQLExecDirect(hstmt, (SQLCHAR*)"ROLLBACK", SQL_NTS);
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
        return;
    }
    SQLFetch(hstmt);
    if(!check) {
        printf("OrderID is not actual order\n");
        SQLExecDirect(hstmt, (SQLCHAR*)"ROLLBACK", SQL_NTS);
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);

    }
    SQLFreeStmt(hstmt, SQL_CLOSE);
    // before doing this need to make sure ship hasn't happened already
    insert = (SQLCHAR*)"SELECT ShippedDate FROM Orders WHERE OrderID=?";
    SQLPrepare(hstmt, insert, SQL_NTS);
    SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &oid, 0, NULL);
    SQL_TIMESTAMP_STRUCT shipped_date_before;
     memset(&shipped_date_before, 0, sizeof(shipped_date_before));

    SQLBindCol(hstmt, 1, SQL_C_TYPE_TIMESTAMP, &shipped_date_before, sizeof(shipped_date_before), NULL);
    ret = SQLExecute(hstmt);
    if(!SQL_SUCCEEDED(ret)) {
        SQLExecDirect(hstmt, (SQLCHAR*)"ROLLBACK", SQL_NTS);
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
        return;
    }
    SQLFetch(hstmt);
    if(shipped_date_before.year!=0) {
        printf("Order already shipped\n");
        SQLExecDirect(hstmt, (SQLCHAR*)"ROLLBACK", SQL_NTS);
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
        return;
    }

    // go through each product in the order and see if there are sufficient products
    SQLFreeStmt(hstmt, SQL_CLOSE);
    insert = (SQLCHAR*)"SELECT ID, ProductID, Quantity FROM Order_Details WHERE OrderID=?";
    SQLPrepare(hstmt, insert, SQL_NTS);
    SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &oid, 0, NULL);
    SQLINTEGER odid;
    SQLINTEGER odpid;
    SQL_NUMERIC_STRUCT od_quantity;
    SQLBindCol(hstmt, 1, SQL_INTEGER, &odid, sizeof(odid), NULL);
    SQLBindCol(hstmt, 2, SQL_INTEGER, &odpid, sizeof(odpid), NULL);
    SQLBindCol(hstmt, 3, SQL_C_NUMERIC, &od_quantity, sizeof(od_quantity), NULL);
    SQLExecute(hstmt);

    SQLHSTMT hstmt2;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &hstmt2);
    SQLINTEGER od_transaction_type = 2;
    SQL_TIMESTAMP_STRUCT ship_date = getSQLNow(dbc);
    SQLExecDirect(hstmt2, (SQLCHAR*)"BEGIN", SQL_NTS);
    while(SQL_SUCCEEDED(SQLFetch(hstmt))) {
        // figure out if the read product has enough in stock
        double od_quantity_d = getDoubleFromSQLNumericStruct(&od_quantity);
        SQLINTEGER od_quantity_INT = (SQLINTEGER) round(od_quantity_d);
        for(int i = 0; i < total_products; i++) {
            if(products_available[i]->pid == odpid) {
                // not enough 
                if(products_available[i]->quantity_avail < od_quantity_d) {
                    printf("Rolling Back, not enough product for %d\n", odpid);
                    SQLExecDirect(hstmt, (SQLCHAR*)"ROLLBACK", SQL_NTS);
                    SQLExecDirect(hstmt2, (SQLCHAR*)"ROLLBACK", SQL_NTS);
                    SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
                    SQLFreeHandle(SQL_HANDLE_STMT, hstmt2);
                    return;
                }
                else {
                    // insert into inventory_transactions
                    SQLFreeStmt(hstmt2, SQL_CLOSE);
                    SQLCHAR* insert2 = (SQLCHAR*)"INSERT INTO Inventory_Transactions(TransactionType, TransactionCreatedDate, TransactionModifiedDate, ProductID, Quantity) VALUES (?, ?, ?, ?, ?)";
                    SQLPrepare(hstmt2, insert2, SQL_NTS);
                    SQLBindParameter(hstmt2, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &od_transaction_type, 0, NULL);
                    SQLBindParameter(hstmt2, 2, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP, SQL_TYPE_TIMESTAMP, 0, 0, &ship_date, 0, NULL);
                    SQLBindParameter(hstmt2, 3, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP, SQL_TYPE_TIMESTAMP, 0, 0, &ship_date, 0, NULL);
                    SQLBindParameter(hstmt2, 4, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &odpid, 0, NULL);
                    SQLBindParameter(hstmt2, 5, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &od_quantity_INT, 0, NULL);
                    SQLRETURN ret = SQLExecute(hstmt2);
                    if(!SQL_SUCCEEDED(ret)) {
                        printf("Rolling Back bad insert into transactions\n");
                        SQLExecDirect(hstmt2, (SQLCHAR*)"ROLLBACK", SQL_NTS);
                        SQLExecDirect(hstmt, (SQLCHAR*)"ROLLBACK", SQL_NTS);
                        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
                        SQLFreeHandle(SQL_HANDLE_STMT, hstmt2);
                        return;
                    }
                }
            }
        }
    }
    
    // TODO if there are enough products fill in ShippedDate, ShipperID, ShippingFee
    SQLFreeStmt(hstmt, SQL_CLOSE);
    insert = (SQLCHAR*)"UPDATE Orders SET ShippedDate=?, ShipperID=?, ShippingFee=? WHERE OrderID=?";
    SQLPrepare(hstmt, insert, SQL_NTS);
    SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_TYPE_TIMESTAMP, SQL_TYPE_TIMESTAMP, 0, 0, &ship_date, 0, NULL);
    printf("Enter the shippingID: ");
    SQLINTEGER* shipping_id = readAndBindInteger(hstmt, 2);
    printf("Enter the shipping Fee: ");
    SQL_NUMERIC_STRUCT* shipping_fee = readAndBindDecimal(hstmt, 3, 16, 4);
    SQLBindParameter(hstmt, 4, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, &oid, 0, NULL);
    ret = SQLExecute(hstmt);
    if(!SQL_SUCCEEDED(ret)) {
        printf("Rolling Back bad update\n");
        SQLExecDirect(hstmt2, (SQLCHAR*)"ROLLBACK", SQL_NTS);
        SQLExecDirect(hstmt, (SQLCHAR*)"ROLLBACK", SQL_NTS);
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt2);
        return;
    }
    SQLExecDirect(hstmt, (SQLCHAR*)"COMMIT", SQL_NTS);
    SQLExecDirect(hstmt2, (SQLCHAR*)"COMMIT", SQL_NTS);

    SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
    SQLFreeHandle(SQL_HANDLE_STMT, hstmt2);
    free(input);
}

void printPendingOrders(SQLHDBC dbc) {
    char* input = NULL;
    size_t n = 0;
    // handle setup and starting transaction
    SQLHSTMT hstmt;
    SQLAllocHandle(SQL_HANDLE_STMT, dbc, &hstmt);
    SQLExecDirect(hstmt, (SQLCHAR*)"BEGIN", SQL_NTS);
    // TODO 
    SQLCHAR* insert = (SQLCHAR*)"SELECT OrderID, EmployeeID, CustomerID, OrderDate FROM Orders WHERE ShippedDate IS NULL ORDER BY OrderDate";
    SQLExecDirect(hstmt, insert, SQL_NTS);
    SQLINTEGER oid;
    SQLINTEGER eid;
    SQLINTEGER cid;
    SQL_TIMESTAMP_STRUCT date_time;
    SQLBindCol(hstmt, 1, SQL_INTEGER, &oid, sizeof(oid), NULL);
    SQLBindCol(hstmt, 2, SQL_INTEGER, &eid, sizeof(eid), NULL);
    SQLBindCol(hstmt, 3, SQL_INTEGER, &cid, sizeof(cid), NULL);
    SQLBindCol(hstmt, 4, SQL_C_TYPE_TIMESTAMP, &date_time, sizeof(date_time), NULL);

    // for each order
    SQLINTEGER pid;
    SQLINTEGER odid;
    SQL_NUMERIC_STRUCT quantity;
    SQL_NUMERIC_STRUCT unit_price;
    SQL_NUMERIC_STRUCT discount;
    while(SQL_SUCCEEDED(SQLFetch(hstmt))) {
        // print "Orders" information
        printf("~~~~~~~~~~~~~~~\n");
        printf("Card %d", oid);
        printf(" %04d-%02d-%02d %02d:%02d:%02d\n",
           date_time.year, date_time.month, date_time.day,
           date_time.hour, date_time.minute, date_time.second);
        printf("EID: %d\n", eid);
        printf("CID: %d\n\n", cid);
        // print for each order_details

        SQLHSTMT hstmt2;
        SQLAllocHandle(SQL_HANDLE_STMT, dbc, &hstmt2);
        SQLCHAR* insert2 = (SQLCHAR*)"SELECT ID, ProductID, Quantity, UnitPrice, Discount \
                           FROM Order_Details WHERE OrderID=?";
        SQLPrepare(hstmt2, insert2, SQL_NTS);
        SQLBindParameter(hstmt2, 1, SQL_PARAM_INPUT, SQL_C_SLONG, SQL_INTEGER, 0, 0, 
                         &oid, 0, NULL);

        SQLBindCol(hstmt2, 1, SQL_INTEGER, &pid, sizeof(pid), NULL);
        SQLBindCol(hstmt2, 2, SQL_INTEGER, &odid, sizeof(odid), NULL);
        SQLBindCol(hstmt2, 3, SQL_C_NUMERIC, &quantity, sizeof(quantity), NULL);
        SQLBindCol(hstmt2, 4, SQL_C_NUMERIC, &unit_price, sizeof(unit_price), NULL);
        SQLBindCol(hstmt2, 5, SQL_C_NUMERIC, &discount, sizeof(discount), NULL);
        SQLExecute(hstmt2);
        while(SQL_SUCCEEDED(SQLFetch(hstmt2))) {
            double quantity_d = getDoubleFromSQLNumericStruct(&quantity);
            double unit_price_d = getDoubleFromSQLNumericStruct(&unit_price);
            double discount_d = getDoubleFromSQLNumericStruct(&discount);
            printf("Order Details ID: %d\n", odid);
            printf("Product ID of: %d\n", pid);
            printf("Quantity: %.2f UnitPrice: %.2f, Discount: %.2f\n", quantity_d, unit_price_d, discount_d);
        }
    }

    SQLExecDirect(hstmt, (SQLCHAR*)"COMMIT", SQL_NTS);

    SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
    free(input);
}
void printMoreOptions() {
    printf("No other features added.\n\n");
}
void userLoop(SQLHDBC dbc) {
    char * input = NULL;
    size_t n = 0;
    for(;;) {
        printf("1. add a customer\n");
        printf("2. add an order\n");
        printf("3. remove an order\n");
        printf("4. ship an order\n");
        printf("5. print pending orders (not shipped yet) with customer information\n");
        printf("6. more options\n");
        printf("7. exit\n");
        printf("Enter number of choice you want to make: ");
        getline(&input, &n, stdin);
        // remove the newline
        input[strlen(input)-1] = 0;
        if(strcmp(input, "1") == 0) {
            addCustomer(dbc);
        }
        else if(strcmp(input, "2") == 0) {
            addOrder(dbc);
        }
        else if(strcmp(input, "3") == 0) {
            deleteOrder(dbc);
        }
        else if(strcmp(input, "4") == 0) {
            shipOrder(dbc);
        }
        else if(strcmp(input, "5") == 0) {
            printPendingOrders(dbc);
        }
        else if(strcmp(input, "6") == 0) {
            printMoreOptions();
        }
        else if(strcmp(input, "7") == 0) {
            free(input);
            return;
        }
        else {
            printf("Invalid input. Expecting soley number like \"1\" \n\n", input);
        }

    }
}
int main() {
    SQLHENV env;
    SQLHDBC dbc;

    // allocate the environment handle
    SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &env);
    // set the ODBC version to ODBC3
    SQLSetEnvAttr(env, SQL_ATTR_ODBC_VERSION, (void *) SQL_OV_ODBC3, 0);
    // allocate connection to northwind
    SQLAllocHandle(SQL_HANDLE_DBC, env, &dbc);
    // actually connect to northwind
    SQLDriverConnect(dbc, NULL, "DSN=mydsn;", SQL_NTS, 
                     NULL, 0, NULL, SQL_DRIVER_COMPLETE);

    userLoop(dbc);

    SQLDisconnect(dbc);
    SQLFreeHandle(SQL_HANDLE_DBC, dbc);
    SQLFreeHandle(SQL_HANDLE_ENV, env);

    return 0;
}
