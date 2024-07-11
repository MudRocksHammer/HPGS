#ifndef __HPGS_NONCOPYABLE_H__
#define __HPGS_NONCOPYABLE_H__


namespace HPGS{

class Noncopyable{
public:
    Noncopyable() = default;
    ~Noncopyable() = default;
    //禁止拷贝构造和赋值拷贝构造
    Noncopyable(const Noncopyable&) = delete;
    Noncopyable& operator=(const Noncopyable&) = delete;
};

}

#endif