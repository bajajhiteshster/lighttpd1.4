SET(T_FILES
	prepare.sh
	cachable.t
	core-404-handler.t
	core-condition.t
	core-keepalive.t
	core-request.t
	core-response.t
	core.t
	core-var-include.t
	lowercase.t
	mod-access.t
	mod-auth.t
	mod-cgi.t
	mod-compress.t
	mod-extforward.t
	mod-fastcgi.t
	mod-proxy.t
	mod-redirect.t
	mod-rewrite.t
	mod-secdownload.t
	mod-setenv.t
	mod-ssi.t
	mod-userdir.t
	request.t
	symlink.t
)

FOREACH(it ${T_FILES})
  ADD_TEST(${it} "${lighttpd_SOURCE_DIR}/tests/wrapper.sh"
		"${lighttpd_SOURCE_DIR}/tests"
		"${lighttpd_BINARY_DIR}"
		"${lighttpd_SOURCE_DIR}/tests/${it}")
ENDFOREACH(it)
