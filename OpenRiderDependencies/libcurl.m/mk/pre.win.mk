# MSVC output (cl.exe, clang-cl): no package provides curl for it, so fetch the
# release and compile it here, with Windows' own TLS (Schannel) and only the
# protocols OpenRider uses: HTTP(S), and file: for local tiles. Its generated
# header, written for this output, comes from patches/msvc-config.patch. No zlib: tiles are served
# compressed already. Static: users define CURL_STATICLIB (the workspace's
# mk/pre.win.mk), and LIBS= hands them the Windows libraries it needs.
# Overrides the makefile's pkg-config import.
.if ${TOOLCHAIN} == "msvc" || ${ABI:U} == "msvc"
IMPORT=fetch:curl
REQUIRES=
FETCH_URL=https://curl.se/download/curl-8.23.0.tar.xz
FETCH_PATCHES=msvc-config.patch
SRCS=lib/altsvc.c lib/amigaos.c lib/api.c lib/bufq.c lib/bufref.c \
     lib/cf-h1-proxy.c lib/cf-h2-proxy.c lib/cf-haproxy.c \
     lib/cf-https-connect.c lib/cfilters.c lib/cf-ip-happy.c \
     lib/cf-recvbuf.c lib/cf-setup.c lib/cf-socket.c lib/conncache.c \
     lib/connect.c lib/content_encoding.c lib/cookie.c lib/creds.c \
     lib/cshutdn.c lib/curl_addrinfo.c lib/curl_ed25519.c lib/curl_endian.c \
     lib/curl_fnmatch.c lib/curl_fopen.c lib/curl_get_line.c \
     lib/curl_gethostname.c lib/curl_gssapi.c lib/curl_memrchr.c \
     lib/curl_ntlm_core.c lib/curl_range.c lib/curl_sasl.c \
     lib/curl_sha512_256.c lib/curl_share.c lib/curl_sspi.c \
     lib/curl_threads.c lib/curl_trc.c lib/curlx/base64.c \
     lib/curlx/basename.c lib/curlx/dynbuf.c lib/curlx/inet_ntop.c \
     lib/curlx/inet_pton.c lib/curlx/multibyte.c lib/curlx/nonblock.c \
     lib/curlx/snprintf.c lib/curlx/strcopy.c lib/curlx/strdup.c \
     lib/curlx/strerr.c lib/curlx/strparse.c lib/curlx/timediff.c \
     lib/curlx/timeval.c lib/curlx/version_win32.c lib/curlx/wait.c \
     lib/curlx/warnless.c lib/curlx/win32-fopen.c lib/curlx/winapi.c \
     lib/cw-out.c lib/cw-pause.c lib/dict.c lib/dllmain.c lib/dynhds.c \
     lib/easy.c lib/easygetopt.c lib/easyoptions.c lib/escape.c \
     lib/fake_addrinfo.c lib/file.c lib/fileinfo.c lib/formdata.c lib/ftp.c \
     lib/ftplistparser.c lib/getenv.c lib/getinfo.c lib/gopher.c lib/hash.c \
     lib/headers.c lib/hmac.c lib/hsts.c lib/http.c lib/http_aws_sigv4.c \
     lib/http_chunks.c lib/http_digest.c lib/http_httpsig.c \
     lib/http_negotiate.c lib/http_ntlm.c lib/http_proxy.c lib/http1.c \
     lib/http2.c lib/idn.c lib/if2ip.c lib/imap.c lib/ldap.c lib/llist.c \
     lib/macos.c lib/md4.c lib/md5.c lib/memdebug.c lib/mime.c \
     lib/mprintf.c lib/mqtt.c lib/multi.c lib/multi_ev.c lib/multi_ntfy.c \
     lib/netrc.c lib/openldap.c lib/parsedate.c lib/peer.c lib/pingpong.c \
     lib/pop3.c lib/progress.c lib/protocol.c lib/proxy.c lib/psl.c \
     lib/ptrarray.c lib/rand.c lib/ratelimit.c lib/request.c lib/rtsp.c \
     lib/select.c lib/sendf.c lib/setopt.c lib/sha256.c lib/slist.c \
     lib/smtp.c lib/socketpair.c lib/socks.c lib/socks_gssapi.c \
     lib/socks_sspi.c lib/splay.c lib/strcase.c lib/strequal.c \
     lib/strerror.c lib/telnet.c lib/tftp.c lib/thrdpool.c lib/thrdqueue.c \
     lib/transfer.c lib/u32_ptrset.c lib/u8_strset.c lib/uint-bset.c \
     lib/uint-spbset.c lib/uint-table.c lib/url.c lib/urlapi.c \
     lib/vauth/cleartext.c lib/vauth/cram.c lib/vauth/digest.c \
     lib/vauth/digest_sspi.c lib/vauth/gsasl.c lib/vauth/krb5_gssapi.c \
     lib/vauth/krb5_sspi.c lib/vauth/ntlm.c lib/vauth/ntlm_sspi.c \
     lib/vauth/oauth2.c lib/vauth/spnego_gssapi.c lib/vauth/spnego_sspi.c \
     lib/vauth/vauth.c lib/vdns/asyn-ares.c lib/vdns/asyn-base.c \
     lib/vdns/asyn-thrdd.c lib/vdns/cf-dns.c lib/vdns/dnscache.c \
     lib/vdns/doh.c lib/vdns/hostip.c lib/vdns/hostip4.c lib/vdns/hostip6.c \
     lib/vdns/httpsrr.c lib/version.c lib/vquic/capsule.c \
     lib/vquic/cf-capsule.c lib/vquic/cf-ngtcp2.c lib/vquic/cf-ngtcp2-cmn.c \
     lib/vquic/cf-ngtcp2-proxy.c lib/vquic/cf-quiche.c lib/vquic/vquic.c \
     lib/vquic/vquic-tls.c lib/vssh/libssh.c lib/vssh/libssh2.c \
     lib/vssh/vssh.c lib/vtls/apple.c lib/vtls/cipher_suite.c \
     lib/vtls/gtls.c lib/vtls/hostcheck.c lib/vtls/keylog.c \
     lib/vtls/mbedtls.c lib/vtls/openssl.c lib/vtls/rustls.c \
     lib/vtls/schannel.c lib/vtls/schannel_verify.c lib/vtls/vtls.c \
     lib/vtls/vtls_config.c lib/vtls/vtls_scache.c lib/vtls/vtls_spack.c \
     lib/vtls/wolfssl.c lib/vtls/x509asn1.c lib/ws.c 
LIB_SHARED=NO
LIBS=ws2_32 crypt32 bcrypt secur32 iphlpapi advapi32
WARN=none
CFLAGS += -I${.CURDIR}/work/_resolved/include -I${.CURDIR}/work/_resolved/lib
CFLAGS += -DHAVE_CONFIG_H -DBUILDING_LIBCURL -DUSE_SCHANNEL -DUSE_WINDOWS_SSPI
CFLAGS += -DCURL_DISABLE_DICT -DCURL_DISABLE_FTP -DCURL_DISABLE_GOPHER \
          -DCURL_DISABLE_IMAP -DCURL_DISABLE_LDAP -DCURL_DISABLE_LDAPS \
          -DCURL_DISABLE_MQTT -DCURL_DISABLE_POP3 -DCURL_DISABLE_RTSP \
          -DCURL_DISABLE_SMB -DCURL_DISABLE_SMTP -DCURL_DISABLE_TELNET \
          -DCURL_DISABLE_TFTP -DCURL_DISABLE_WEBSOCKETS
.endif
