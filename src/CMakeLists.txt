# DoxMail: with WITH_GTK4 only the directories already ported to GTK 4 are
# built; add each one here as it compiles (doxmail/PORTING-GTK4.md).
if(WITH_GTK4)
	add_subdirectory(libgnomecanvas)
	add_subdirectory(e-util)
	add_subdirectory(doxmail-tests)
	return()
endif(WITH_GTK4)

add_subdirectory(addressbook)
add_subdirectory(calendar)
add_subdirectory(composer)
add_subdirectory(em-format)
add_subdirectory(e-util)
add_subdirectory(libemail-engine)
add_subdirectory(libgnomecanvas)
add_subdirectory(mail)
add_subdirectory(modules)
add_subdirectory(plugins)
add_subdirectory(shell)
add_subdirectory(web-extensions)

if(WITH_GLADE_CATALOG)
	add_subdirectory(glade)
endif(WITH_GLADE_CATALOG)

if(ENABLE_SMIME)
	add_subdirectory(smime)
endif(ENABLE_SMIME)
