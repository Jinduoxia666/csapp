#! /usr/bin/perl -w
use strict;
use Digest::MD5;
#
# port-for-user.pl —— 为指定用户生成冲突概率较低的端口 p。
# 端口 p 始终为偶数，因此可以将 p 和 p+1
# 分别用于代理与 Tiny Web 服务器的测试。
# 根据用户名稳定地选择端口。
#     
# 用法：./port-for-user.pl [可选的用户名]
#
my $maxport = 65536;
my $minport = 1024;


# hashname —— 根据参数的哈希值计算一个偶数端口
sub hashname {
    my $name = shift;
    my $port;
    my $hash = Digest::MD5::md5_hex($name);
    # 仅取最后 32 位，即末尾 8 个十六进制数字
    $hash = substr($hash, -8);
    $hash = hex($hash);
    $port = $hash % ($maxport - $minport) + $minport;
    $port = $port & 0xfffffffe;
    print "$name: $port\n";
}


# 未提供命令行参数时，对当前用户名计算哈希；
# 否则对各个命令行参数计算哈希。
if($#ARGV == -1) {
    my ($username) = getpwuid($<);
    hashname($username);
} else {
    foreach(@ARGV) {
        hashname($_);
    }
}
