#include"http.hpp"
#include"serve.hpp"
int main(){
    Serve srv;
    srv.Start(8000);
    return 0;
}
