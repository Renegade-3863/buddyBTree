#include <stdlib.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

// size代表分配器管理的内存单元数目
// longest数组用于记录一棵二叉树
struct buddy2 {
    unsigned size;
    unsigned longest[1];
};

// 宏定义用于简化代码, 分别用于计算某一结点的父结点和左右孩子结点
#define LEFT_LEAF(index) ((index) * 2 + 1)
#define RIGHT_LEAF(index) ((index) * 2 + 2)
#define PARENT(index) (((index)+1)/2-1)


// 用于判断某一个值是否为2的幂数
// 解释:
// x & (x-1)的结果是将x最右侧的一个1变为0
// 例如:
//     010101000 & 010100111 = 010100000
// 而我们知道一个数如果是2的n次方, 那么它的二进制表示的最高位为1, 其余位为0
// 下面的方法就是利用了这两条性质快速进行了2的幂数的判断
#define IS_POWER_OF_2(x) (!((x)&((x)-1)))
// 取两数中较大者的宏定义
#define MAX(a, b) ((a) > (b) ? (a) : (b))

// 如宏
#define ALLOC malloc
#define FREE free

// 将输入的无符号整数size向上取到最接近的2的幂次方的值, 并返回结果
// 个人认为这个实现不太容易理解, 换了一种方式实现
static unsigned fixsize(unsigned size) {
    // size |= size >> 1;
    // size |= size >> 2;
    // size |= size >> 4;
    // size |= size >> 8;
    // size |= size >> 16;
    // return size+1;
    int idx = 0;
    while(size) {
        size = size >> 1;
        ++idx;
    }
    // 结束循环后idx记录的是size中最左侧的1的下标
    return (1 << idx);
}

// 创建一个分配器对象, 用于后续调用进行内存分配
struct buddy2* buddy2_new(int size) {
    struct buddy2* self;
    unsigned node_size;
    int i;

    // 这里的size默认为2的幂数, 因为伙伴系统分配的内存块常为2的幂数大小, 这里的size代表整个系统可以分配的最大内存块大小 
    if(size < 1 || !IS_POWER_OF_2(size))
        return NULL;

    // 分配对应的内存空间
    // 初始分配两倍size大小的unsigned大小空间给self, 2*size-1块unsigned给longest, 余下一块给size
    // 共2^(log(size)+1)-1个二叉树结点需要管理, 余下的一个是size本身(也是一个unsigned类型变量)的空间
    // 这里又有一个小小的细节, C语言的malloc函数分配空间是取出连续的地址块进行分配, 也就是说, longest数组的初始大小在前面定义中可以是任意一个[0, 2*size-2]之间的值
    // longest[i]的访问是从longest指向的第一个位置向后挪动i个unsigned类型大小实现的
    self = (struct buddy2*)ALLOC(2*size*sizeof(unsigned));
    self->size = size;
    // 先乘个2, pad后面for循环的逻辑
    node_size = size * 2;

    // 这个for循环很好理解
    for(i = 0; i < 2 * size - 1; ++i) {
        // 如果第i个结点出现在了新的一层上, 那么就更新结点大小
        if(IS_POWER_OF_2(i+1))
            node_size /= 2;
        // 进行结点大小赋值
        self->longest[i] = node_size;
    }
    return self;
}

// 简单free掉分给分配器的内存空间
void buddy2_destroy(struct buddy2* self) {
    FREE(self);
}

// 使用分配器进行内存分配, 请求的内存大小为size字节
// 返回值为内存块的索引值
int buddy2_alloc(struct buddy2* self, int size) {
    unsigned index = 0;
    unsigned node_size;
    unsigned offset = 0;

    if(self == NULL) 
        return -1;
    
    if(size <= 0)
        return -1;
    // 如果申请的内存不是恰好一块内存的大小
    // 则要保证至少分配一块大小足够的内存空间
    else if(!IS_POWER_OF_2(size))
        size = fixsize(size);
    
    // 如果最大可用的内存块都小于这个理应分配的大内存块的大小或者已经被占用了(值为0), 那么无法成功分配, 返回-1
    if(self->longest[index] < size) 
        return -1;
    
    // for循环模拟DFS查找满二叉树的过程, node_size代表当前遍历的结点监控的内存块的大小(如果空闲), 如果非空闲, 则对应的longest[index]值为0
    for(node_size = self->size; node_size != size; node_size /= 2) {
        // 如果左子结点还有空位, 则向左深入一层继续探查更小的空闲结点
        if(self->longest[LEFT_LEAF(index)] >= size)
            index = LEFT_LEAF(index);
        // 否则, 左子树没有满足条件的内存空间, 只能向右子树深入寻找
        else 
            index = RIGHT_LEAF(index);
    }
    // 进行分配, 将该内存块标记为0(被占用)
    self->longest[index] = 0;
    // 计算内存大块内部的第一个内存基本单元(大小为1个字节的内存空间)在整个大块内存中的位置偏移
    // 证明如下:
    // 假设index结点所在的层为i层(从上到下依次为0-i层)
    // 那么index所在的层应该有2^i个结点, 即有self->size == 2^i * node_size
    // 而index = 2^(i-1)-1+t, 其中t就是该结点在所在层的偏移量(取值为0~2^i-1)
    // 这样的话, 我们可以推导出:
    // 该内存块的第一个字节的偏移量应该是t*node_size
    // 那么我们就可以得出这样的式子:
    // t*node_size = ((index+1)*node->size-self->size) = offset
    offset = (index + 1) * node_size - self->size;

    // 向上回溯路径, 更新对应的监控结点视角下剩余的最大内存块
    while (index) {
        index = PARENT(index);
        self->longest[index] = MAX(self->longest[LEFT_LEAF(index)], self->longest[RIGHT_LEAF(index)]);
    }

    return offset;
}

// 使用buddy2分配器释放一块索引为offset位置的内存块
void buddy2_free(struct buddy2* self, int offset) {
    // index记录当前遍历的结点下标
    unsigned node_size, index = 0;
    unsigned left_longest, right_longest;
    // 确认self存在, 并且偏移量合法
    assert(self && offset >= 0 && offset < self->size);

    // 从分配的内存块的第一个结点开始向上搜索
    node_size = 1;
    index = offset + self->size - 1;
    // 找到一个为0的结点, 代表获取了了当前分配的这个内存块的大小信息, 信息存储在node_size中
    for(; self->longest[index]; index = PARENT(index)) {
        node_size *= 2;
        // index等于0了说明self->longest[0] != 0
        // 这意味着实际上这块内存并没有被分配出去, 所以不能被释放
        // 这里直接进行返回
        if(index == 0)
            return;
    }
    // 将longest恢复原来大小, 模拟释放内存操作
    self->longest[index] = node_size;

    // 继续向上回溯, 检查是否存在可合并的块
    while(index) {
        index = PARENT(index);
        node_size *= 2;

        left_longest = self->longest[LEFT_LEAF(index)];
        right_longest = self->longest[RIGHT_LEAF(index)];

        if(left_longest + right_longest == node_size)
            self->longest[index] = node_size;
        else 
            self->longest[index] = MAX(left_longest, right_longest);
    }
}

// 获取索引值为offset的内存块的大小
int buddy2_size(struct buddy2* self, int offset) {
    unsigned node_size, index = 0;
    
    assert(self && offset >= 0 && offset < self->size);

    node_size = 1;
    for(index = offset + self->size - 1; self->longest[index]; index = PARENT(index))
        node_size *= 2;
    return node_size;
}

void buddy2_dump(struct buddy2* self) {
    char canvas[65];
    int i, j;
    unsigned node_size, offset;

    if(self == NULL) {
        printf("buddy2_dump: (struct buddy2*)self == NULL");
        return;
    }

    if(self->size > 64) {
        printf("buddy2_dump: (struct buddy2*)self is too big to dump");
        return;
    }

    memset(canvas, '_', sizeof(canvas));
    node_size = self->size * 2;

    for(i = 0; i < 2 * self->size - 1; ++i) {
        if(IS_POWER_OF_2(i+1))
            node_size /= 2;
        
        if(self->longest[i] == 0) {
            if(i >= self->size - 1)
                canvas[i - self->size + 1] = '*';
            else if(self->longest[LEFT_LEAF(i)] && self->longest[RIGHT_LEAF(i)]) {
                offset = (i+1) * node_size - self->size;

                for(j = offset; j < offset + node_size; ++j)
                    canvas[j] = '*';
            }
        }
    }
    canvas[self->size] = '\0';
    puts(canvas);
}