module main
use stdio

func main() {
    buf char[9] = {0,0,0,0,0,0,0,0,0}
    bp void = &buf
    /* 切片右界 b 是最后一个可写字节：9 字节容不下 10 字符 + 结尾 NUL（§5.1.4） */
    bp.(char[9])[0..8] = "0123456789"
    puts(buf)
}
