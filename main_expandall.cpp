#include <iostream>
#include <string>
#include<cmath>
#include<cstdlib>
#include <cstring>
#include <ctime>
#include "jsoncpp/json.h"
using namespace std;

#define MaxUcb 10000000;
#define board_size 9  // 棋盘大小: 9 * 9
int Board[board_size][board_size];  // 初始棋盘
bool dfs_air_visit[board_size][board_size];  // 存储每个点是否被访问过
const int cx[]={-1,0,1,0};
const int cy[]={0,-1,0,1};
typedef pair<int,int> Action;  // 表示落子位置

const double C = 1.4;  // 权重(UCB公式中的C)
int MyColor;  // 我方棋子颜色
int ComputeTime = 600000;  // 计算的时间限制
int IterationTimes;  // 保存搜索过程中的迭代次数
double WinRate;  // 计算胜率
int SimulateDepth = 6;  // 模拟搜索的深度

inline bool inBoarder(int x, int y)
{
    return x>=0 && y>=0 && x < board_size && y < board_size;
}

//true: has air
inline bool hasAir(int board[9][9], int fx, int fy)  //无需优化
{
    dfs_air_visit[fx][fy] = true;
    bool flag=false;
    for (int dir = 0; dir < 4; dir++)
    {
        int dx=fx+cx[dir], dy=fy+cy[dir];
        if (inBoarder(dx, dy))
        {
            if (board[dx][dy] == 0)
                flag=true;
            if (board[dx][dy] == board[fx][fy] && !dfs_air_visit[dx][dy])
                if (hasAir(board, dx, dy))
                    flag=true;
        }
    }
    return flag;
}

//true: available
inline bool judgeAvailable(int board[board_size][board_size],int fx, int fy, int col)  //无需优化
{
    if (board[fx][fy]) return false;
    board[fx][fy] = col;
    memset(dfs_air_visit, 0, sizeof(dfs_air_visit));
    if (!hasAir(board,fx, fy))
    {
        board[fx][fy]  = 0;
        return false;
    }
    for (int dir = 0; dir < 4; dir++)
    {
        int dx=fx+cx[dir], dy=fy+cy[dir];
        if (inBoarder(dx, dy))
        {
            if (board[dx][dy] && !dfs_air_visit[dx][dy])
                if (!hasAir(board,dx, dy))
                {
                    board[fx][fy]  = 0;
                    return false;
                }
        }
    }
    board[fx][fy]  = 0;
    return true;
}

//---------策略部分---------

typedef struct Node  // 蒙特卡洛搜索树结点
{
    /* 棋盘属性 */
    int board[board_size][board_size];  // 棋盘
    int col;  // 该棋盘下一步的颜色(1为白，-1为黑)
    int round;  // 该棋盘下一步的回合数
    Action action;  // 该棋盘上一步的落子位置
    double value;  // 该状态的价值

    /* 结点属性 */
    int visited_times;  // 该结点的搜索次数
    Node *parent;  // 父节点;
    Node *childs[board_size][board_size];  // 子节点指针数组,新建的孩子结点需要存在对应下标的位置
    int child_num;  // 子节点数量
} Node;

// 判断是否为终止状态
inline bool IsTerminal(int board[9][9], int col)   // 无需优化
{
    for (int i=0;i<9;i++)  // 检查棋盘是否能够继续落子，若能则不是终止状态
    {
        for (int j = 0; j < 9; j++)
        {
            if (judgeAvailable(board, i, j, col))
            {
                return false;
            }
        }
    }
    return true;
}

int CountIllegalStep(int board[9][9], int col)  // 返回棋盘上某一方的非法步数
{
    int count = 0;
    for (int i = 0;i < 9; i++)
    {
        for (int j = 0; j < 9; j++)
        {
            if (board[i][j] == 0 && !judgeAvailable(board, i, j, col))  //如果该位置为空但却不可落子，则计数加1
            {
                count++;
            }
        }
    }
    return count;
}

inline void *PutChess(Node *node, Action act)  // 在结点node下棋,返回新结点指针(不管是否能下，交给调用的函数去维护：即Expand函数
{
    Node *new_node = new Node;
    /* 设置棋盘属性 */
    memcpy(new_node->board, node->board, sizeof(node->board));
    new_node->board[act.first][act.second] = node->col;  // 在棋盘上落子
    new_node->col = -node->col;  // 更换下棋方
    new_node->round = node->round + 1;  // 回合+1
    new_node->action.first = act.first, new_node->action.second = act.second;  // 表明上一步的动作

    /* 设置结点属性 */
    new_node->value = 0;  // 初始化价值
    new_node->child_num = 0;
    new_node->visited_times = 0;  // 搜索次数初始为0
    new_node->parent = node;  // 设置父节点
    memset(new_node->childs, 0, sizeof(new_node->childs));
    node->childs[act.first][act.second] = new_node;
    return new_node;
}

// 展开node结点,即将node的下一层的所有博弈树结点加入到树中
inline void ExpandAll(Node *node)
{
    for(int i = 0; i < board_size; i++)
    {
        for(int j = 0; j < board_size; j++)
        {
            if(!node->childs[i][j] && judgeAvailable(node->board, i, j, node->col))
            {
                PutChess(node, Action(i, j));
                node->child_num++;
            }
        }
    }
}

inline Node* ExpandAllandChoose(Node* node)
{
    vector<int> available_list;
    for(int i = 0; i < board_size; i++)
    {
        for(int j = 0; j < board_size; j++)
        {
            if(!node->childs[i][j] && judgeAvailable(node->board, i, j, node->col))
            {
                available_list.push_back(i * 9 + j);
                PutChess(node, Action(i, j));
                node->child_num++;
            }
        }
    }
    if(available_list.empty())
    {
        return node;
    }
    int random_num = rand() % available_list.size();
    int random_choice = available_list[random_num];
    return node->childs[random_choice / 9][random_choice %9];
}

// 返回结点node中UCB值最大的子结点，如果没有子结点，则返回NULL, 如果有多个UCB值相同的子结点，则返回任意一个,用于扩展与模拟
inline Node*BestUCBChild_ExpandALl(Node *node, int status)
{
    double max_ucb = -1000000;
    Node* best_child = nullptr;
    for(int i = 0; i < board_size; i++)
    {
        for(int j = 0; j < board_size; j++)
        {
            if(node->childs[i][j])
            {
                double c;
                double temp_value = node->childs[i][j]->value;
                int temp_visited_times = node->childs[i][j]->visited_times;
                double temp_ucb;
                if(status == 1)
                {
                    c = C;
                }
                else
                {
                    c = 0;
                }
                if(temp_visited_times == 0)
                {
                    temp_ucb = MaxUcb
                }
                else
                {
                    temp_ucb = temp_value / temp_visited_times + c * sqrt(log(node->visited_times) / temp_visited_times);  //这个公式论文中还有优化方法
                }
                if(temp_ucb - max_ucb > 1e-6)
                {
                    max_ucb = temp_ucb;
                    best_child = node->childs[i][j];
                }
                else if(abs(temp_ucb - max_ucb) <= 1e-6)  // 若两者差小于等于1e-6，视作相等，随机选择
                {
                    if(rand() % 2 == 1)  // 二分之一的概率选择他
                    {
                        max_ucb = temp_ucb;
                        best_child = node->childs[i][j];
                    }
                }
            }
        }
    }
    return best_child;
}

inline Node* Select(Node *node)  //选择ucb策略中的叶节点
{
    Node* BestChild = node;
    while(BestChild->child_num)
    {
        BestChild = BestUCBChild_ExpandALl(BestChild, 1);
    }
    return BestChild;  //如果已经是叶节点，返回自己
}

//inline Node *MostVisitChild(Node *node)  //返回访问次数最多的子节点
//{
//    int max = 0;
//    Node* best_child = nullptr;
//    for(int i = 0; i < board_size; i++)
//    {
//        for(int j = 0; j < board_size; j++)
//        {
//            if(node->childs[i][j])
//            {
//                int visit_t = node->childs[i][j]->visited_times;
//                if(visit_t > max)
//                {
//                    max = visit_t;
//                    best_child = node->childs[i][j];
//                }
//                else if(visit_t == max)  // 若两者访问次数相同
//                {
//                    if(rand() % 2 == 1)  // 二分之一的概率选择他
//                    {
//                        max = visit_t;
//                        best_child = node->childs[i][j];
//                    }
//                }
//            }
//        }
//    }
//    return best_child;
//}

// MCTS的Selection和Expansion
//传入当前需要开始搜索的结点,根据exploration/exploitation算法选择最好的需要进行expand的结点
// 具体策略为:从根节点开始向下expand一层，选择ucb值最大的子节点，若该节点不为叶节点，则继续向下搜索ucb值最大的点，若为叶节点且未被模拟过，返回该叶节点进行模拟，否则扩展一层后随机选择一个子节点进行模拟
// 如果全部expand完，则选择自身
inline Node*TreePolicy_ExpandAll(Node *node)
{
    Node*LeafNode = Select(node);  //选择ucb策略中的叶节点
    if(LeafNode->visited_times)  //如果叶节点模拟过对局,扩展一层后随机选一个模拟,如果无法扩展，模拟BestChild;
    {
        return ExpandAllandChoose(LeafNode);
    }
    else  // 如果没有模拟过对局，返回该叶节点
    {
        return LeafNode;
    }
}

//Simulation,模拟depth步后用一定策略进行评估，加快模拟速度，提高迭代次数
//评估策略为计算我方与对方在场上空位置处的总非法步数，非法步数多的一方判负，相同则随机判(因为前期很容易相同，那前期跟随机走差不多，可以考虑在相同的时候更换策略)
//返回对当前结点状态的评估值value，我方胜利,value为1,否则为0
inline double DefaultPolicy(Node *node, int depth)
{
    int temp_board[board_size][board_size];  // 复制node的棋盘用于模拟
    memcpy(temp_board, node->board, sizeof(temp_board));
    int color = node->col;
    int rounds = node->round;
    bool flag = true;  // 标记是否使用评估策略进行评估
    for(int i = 0; i < depth; i++)  // 快速随机落子
    {
        vector<int> available_list; //合法位置表
        for (int i=0;i<9;i++)  //随机选取可落子处落子
            for (int j=0;j<9;j++)
                if (judgeAvailable(temp_board,i,j, color))
                    available_list.push_back(i * 9 + j);
        if(available_list.empty())  // 为最终状态时，直接进行查看颜色进行评估，而不用调用评估策略函数
        {
            flag = false;
            break;
        }
        int result = available_list[rand() % available_list.size()];  //随机选取
        temp_board[result / 9][result % 9] = color;  //落子
        color = -color;  //交换下棋者颜色
        rounds++;  // 回合数增加
    }
    if(!flag)  // 棋盘为最终状态
    {
        if(color == MyColor)  //下一步的那一方为输家
        {
            return 0;
        }
        else
        {
            return 1;
        }
    }
    else  // 棋盘非最终状态
    {
        int one_illegal = CountIllegalStep(temp_board, MyColor);
        int two_illegal = CountIllegalStep(temp_board, -MyColor);
        if(one_illegal < two_illegal)  //己方非法步数少,视为获胜
        {
            return 1;
        }
        else if(one_illegal > two_illegal)
        {
            return 0;
        }
        else  // 相同则随机返回/更换策略(有待寻求策略)
        {
            return rand() % 2;
        }
    }
}

inline void BackUp(Node *node, double val)  //BackPropagation
{
    while(node)
    {
        node->value += val;  // 价值增加
        node->visited_times++;  // 访问次数加1
        node = node->parent;  // BackPropagate
    }
}

inline Action MCTS(Node *node)
{
    double start = clock();
    int i;
    Node * expand_node = node;
    ExpandAll(node);  // 先扩展出所有子结点
    for(i  = 0; i < 12330; i++)
    {
        expand_node  = TreePolicy_ExpandAll(node);  // 选出需要进行模拟的结点
        double value = DefaultPolicy(expand_node, SimulateDepth);
        BackUp(expand_node, value);
    }
    IterationTimes = i;  // 保存迭代次数
    Node *BestNextNode = BestUCBChild_ExpandALl(node, 0);  //返回价值最大的子节点
    WinRate = BestNextNode->value / BestNextNode->visited_times;
    return BestNextNode->action;
}

int main()
{
    srand((unsigned)time(0));
    string str;
    int x,y;
    getline(cin, str);
    Json::Reader reader;
    Json::Value input;
    reader.parse(str, input);
    // 分析自己收到的输入和自己过往的输出，并恢复状态
    memset(Board, 0, sizeof(Board));
    int turnID = input["responses"].size();
    for (int i = 0; i < turnID; i++)
    {
        x=input["requests"][i]["x"].asInt(), y=input["requests"][i]["y"].asInt();
        if(x!=-1) Board[x][y] = 1;
        x=input["responses"][i]["x"].asInt(), y=input["responses"][i]["y"].asInt();
        if (x!=-1) Board[x][y] = -1;
    }
    x=input["requests"][turnID]["x"].asInt(), y=input["requests"][turnID]["y"].asInt();
    if (x!=-1)
    {
        Board[x][y] = 1;
    }
    // 输出决策JSON
    Json::Value ret;
    Json::Value action;

    //以下为MCTS策略
    MyColor = -1;
    Node *root = new Node;  // 根节点
    memcpy(root->board, Board, sizeof(root->board));
    root->round = 1;
    root->action = Action(-1,-1);
    root->value = 0;
    root->col = -1;  // 我方视作黑色

    root->parent = nullptr;
    root->visited_times = 0;
    root->child_num = 0;
    memset(root->childs, 0, sizeof(root->childs));

    Action result = MCTS(root);

    action["x"] = result.first;
    action["y"] = result.second;
    ret["response"] = action;
    char de[100];  // 保存debug信息
    sprintf(de, "模拟次数：%d, 胜率：%.3f", IterationTimes, WinRate);
    ret["debug"] = de;
    Json::FastWriter writer;

    cout << writer.write(ret) << endl;
    return 0;
}