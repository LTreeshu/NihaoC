module main

Person struct {
    age
    score
}

/* struct 返回（聚合变量 return p，sret 缓冲） */
func makePerson() Person {
    p Person = {30, 85}
    return p
}

/* struct 参数按值展开（成员槽逐个 PARAM）+ 标量参数混排 */
func totalScore(p Person, extra i64) i64 {
    return p.score + extra
}

/* 多 struct 参数 */
func sumScores(a Person, b Person) i64 {
    return a.score + b.score
}

/* 窄成员 */
func bump(p Person) Person {
    p.age += 1
    return p
}

func main() {
    /* return p 聚合变量 */
    q Person = makePerson()
    if q.age == 30 && q.score == 85 {
        puts("sret var ok")
    } else {
        puts("sret var bad")
    }
    /* struct 参数（变量） */
    r i64 = totalScore(q, 15)
    if r == 100 {
        puts("struct param ok")
    } else {
        puts("struct param bad")
    }
    /* 链式：mr 调用作实参 */
    s i64 = totalScore(makePerson(), 5)
    if s == 90 {
        puts("sret chain ok")
    } else {
        puts("sret chain bad")
    }
    /* 多 struct 参数 */
    t i64 = sumScores(q, makePerson())
    if t == 170 {
        puts("multi struct ok")
    } else {
        puts("multi struct bad")
    }
    /* struct 修改后返回 */
    u Person = bump(q)
    if u.age == 31 {
        puts("mutate ok")
    } else {
        puts("mutate bad")
    }
    return
}
