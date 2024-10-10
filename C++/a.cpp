#include<cstdio>
#include<algorithm>
#include<cstring>
using namespace std;


int main(){
    float *value = (float *)malloc(sizeof(float));
    int a[10];
    a[0] = 0x3F666666;
    memcpy(value,a,sizeof(float));
    printf("%f\n",*value);
    return 0;
}