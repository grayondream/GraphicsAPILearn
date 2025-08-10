#pragma once
#include "Vector2DBase.hpp"
#include "Vector3DBase.hpp"
#include <cmath>
#include <cstdint>
#include <utility>
#include <algorithm>

template<class T>
class Vector3DBase;

template<class T>
class Vector4DBase{
public:
    using ValueType = T;
    constexpr static const std::size_t Size = 4;
public:
    Vector4DBase(const std::initializer_list<T> &ls){
        assert(Size == ls.size());
        this->x = *(ls.begin());
        this->y = *(ls.begin() + 1);
        this->z = *(ls.begin() + 2);
        this->w = *(ls.begin() + 3);
    }

    Vector4DBase(const Vector4DBase &vec){
        std::copy_n(vec.data, Size, data);
    }

    //从两个点来创建一个向量
    template<typename U, typename K>
    Vector4DBase(const Vector4DBase<U> &rst, const Vector4DBase<K> &snd){
        *this = rst - snd;
    }

    Vector4DBase& operator=(const ValueType &v){
        this->fill(v);
    }
public:
    template<class U>
    Vector4DBase<ValueType>& operator+=(const Vector4DBase<U> &mat){
        return this->foreachFuncBetweenMatrix(mat, [](const T &v1, const U &v2){ return v1 + v2; });
    }

    template<class U>
    Vector4DBase<ValueType>& operator+=(const U &val){
        return this->foreachFuncBinaryValue(val, [](const T &v1, const U &v2){ return v1 + v2; });
    }

    template<class U>
    Vector4DBase<ValueType>& operator-=(const Vector4DBase<U> &mat){
        return this->foreachFuncBetweenMatrix(mat, [](const T &v1, const U &v2){ return v1 - v2; });
    }

    template<class U>
    Vector4DBase<ValueType>& operator-=(const U &val){
        return this->foreachFuncBinaryValue(val, [](const T &v1, const U &v2){ return v1 - v2; });
    }

    template<class U>
    Vector4DBase<ValueType>& operator*=(const Vector4DBase<U> &mat){
        return this->foreachFuncBetweenMatrix(mat, [](const T &v1, const U &v2){ return v1 * v2; });
    }

    template<class U>
    Vector4DBase<ValueType>& operator*=(const U &val){
        return this->foreachFuncBinaryValue(val, [](const T &v1, const U &v2){ return v1 * v2; });
    }

    template<class U>
    Vector4DBase<ValueType>& operator/=(const Vector4DBase<U> &mat){
        return this->foreachFuncBetweenMatrix(mat, [](const T &v1, const U &v2){ return v1 / v2; });
    }

    template<class U>
    Vector4DBase<ValueType>& operator/=(const U &val){
        return this->foreachFuncBinaryValue(val, [](const T &v1, const U &v2){ return v1 / v2; });
    }

    bool operator==(const Vector4DBase &vec) const{
        return vec.x == this->x && vec.y == this->y && vec.z == this->z && vec.w == this->w;
    }

public:
    /*
     * @brief 遍历每个元素，针对当前元素和输入的mat同位置的元素同时执行func，返回的值会写入当前矩阵中
     */
    template<class U, typename Func>
    Vector4DBase<ValueType>& foreachFuncBetweenMatrix(const Vector4DBase<U> &mat, Func &&func){
        this->x = func(this->x, mat.x);
        this->y = func(this->y, mat.y);
        this->z = func(this->z, mat.z);
        this->w = func(this->w, mat.w);
        return *this;
    }

    template<typename Func>
    Vector4DBase<ValueType> &foreachFuncSingleValue(Func &&func){
        this->x = func(this->x);
        this->y = func(this->y);
        this->z = func(this->z);
        this->w = func(this->w);
        return *this;
    }

    template<class U, typename Func>
    Vector4DBase<ValueType> &foreachFuncBinaryValue(const U &u, Func &&func){
        this->x = func(this->x, u);
        this->y = func(this->y, u);
        this->z = func(this->z, u);
        this->w = func(this->w, u);
        return *this;
    }

    template<class U, typename Func>
    U foreachFuncTotal(Func &&func){
        return this->x + this->y + this->z + this->w;
    }
public:
    template<class U>
    U sum(){
        return this->foreachFuncTotal<U>([](const ValueType v1, const ValueType v2){ return v1 + v2; });
    }

    Vector4DBase<ValueType>& fill(const ValueType v = 1 + ValueType{}){
        return this->foreachFuncSingleValue([&v](const ValueType&){ return v; });
    }

    Vector3DBase<T> homogeneous() const{
        return Vector3DBase<T>(*this);
    }

    T length() const {
        return std::sqrt(x * x + y * y + z * z + w * w);
    }

    Vector4DBase& normalize() {
        const auto l = length();
        x /= l;
        y /= l;
        z /= l;
        w /= l;
        return *this;
    }

    Vector4DBase normalize() const{
        return this->normalize();
    }

    template<class U>
    auto dot(const Vector4DBase<U> &vec){
        auto r = *this * vec;
        return r.x + r.y + r.z + r.w;
    }

public:
    union{
        ValueType data[Size];
        struct{
            ValueType x;
            ValueType y;
            ValueType z;
            ValueType w;
        };
        struct{
            ValueType r;
            ValueType g;
            ValueType b;
            ValueType a;
        };
    };
};

template<class T>
auto operator+(const Vector4DBase<T> &m1, const Vector4DBase<T> &m2){
    Vector4DBase<T> ret(m1);
    return ret += m2;
}

template<class T, class U, typename = std::enable_if_t<!std::is_same_v<U, Vector4DBase<T>>>>
auto operator+(const Vector4DBase<T> &m1, const U &val){
    Vector4DBase<T> ret(m1);
    return ret += val;
}

template<class T, class U, typename = std::enable_if_t<!std::is_same_v<U, Vector4DBase<T>>>>
auto operator+(const U &val, const Vector4DBase<T> &m1){
    return m1 + val;
}

template<class T>
auto operator-(const Vector4DBase<T> m1, const Vector4DBase<T> &m2){
    Vector4DBase<T> ret(m1);
    return ret -= m2;
}

template<class T, class U, typename = std::enable_if_t<!std::is_same_v<U, Vector4DBase<T>>>>
auto operator-(const Vector4DBase<T> m1, const U &val){
    Vector4DBase<T> ret(m1);
    return ret -= val;
}

template<class T, class U, typename = std::enable_if_t<!std::is_same_v<U, Vector4DBase<T>>>>
auto operator-(const U &val, const Vector4DBase<T> m1){
    return val + ( -1 * m1);
}

template<class T>
auto operator*(const Vector4DBase<T> m1, const Vector4DBase<T> &m2){
    Vector4DBase<T> ret(m1);
    return ret *= m2;
}

template<class T, class U, typename = std::enable_if_t<!std::is_same_v<U, Vector4DBase<T>>>>
auto operator*(const Vector4DBase<T> m1, const U &val){
    Vector4DBase<T> ret(m1);
    return ret *= val;
}

template<class T, class U, typename = std::enable_if_t<!std::is_same_v<U, Vector4DBase<T>>>>
auto operator*(const U &val, const Vector4DBase<T> m1){
    return m1 * val;
}

template<class T>
auto operator/(const Vector4DBase<T> &m1, const Vector4DBase<T> &m2){
    Vector4DBase<T> ret(m1);
    return ret /= m2;
}

template<class T, class U, typename = std::enable_if_t<!std::is_same_v<U, Vector4DBase<T>>>>
auto operator/(const Vector4DBase<T> &m1,const U &val){
    Vector4DBase<T> ret(m1);
    return ret /= val;
}

template<class T, class U, typename = std::enable_if_t<!std::is_same_v<U, Vector4DBase<T>>>>
auto operator/(const U &val, const Vector4DBase<T> &m1){
    using ReturnType = std::common_type_t<T, U>;
    Vector4DBase<ReturnType> ret(m1);
    ret.fill(ReturnType{} + 1);
    ret *= val;
    return ret / m1;
}

template<class T, class U>
auto Vector4DDot(const Vector4DBase<T> &rst, const Vector4DBase<U> &snd){
    return rst.dot(snd);
}