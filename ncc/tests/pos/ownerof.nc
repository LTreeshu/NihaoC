module main
use stdio

Person struct { name char[] age u8 score i32 }
Data union { b i32 f f64 }
Packet struct { type u8 len u8:4 flag u8:4 data i32 tail u8:3 }

func main() {
    var boy Person = {"x", 13, 100}
    var np void = &boy.name
    if structof(Person, name, np) == &boy {
        puts("structof ok")
    }
    var sp void = &boy.score
    if holdof(Person, score, sp) == &boy {
        puts("holdof ok")
    }

    var u Data = {7}
    var up void = &u.b
    if unionof(Data, b, up) == &u {
        puts("unionof ok")
    }

    /* 位偏移模型：type(u8) 后进入 u8 位域单元（起始字节 1）；
       len 0 位起、flag 第 4 位起；data(i32) 在 C 布局中落在偏移 4，
       tail 的单元起点为 4+4=8 字节 → 64 位 */
    if bitoffsetof(Packet, len) == 8 {
        puts("bitoff len ok")
    }
    if bitoffsetof(Packet, flag) == 12 {
        puts("bitoff flag ok")
    }
    if bitoffsetof(Packet, tail) == 64 {
        puts("bitoff tail ok")
    }
}
