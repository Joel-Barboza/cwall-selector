
Se intentó seguir el tutorial Writting Wayland Clients[^tuto], pero usaba wl_shell y no funcionaba

Y viendo la documentación en The Wayland Protocol [^protocol], se notó que [`wl_shell`](https://wayland.freedesktop.org/docs/html/apa.html#protocol-spec-wl_shell) 
está obsoleto y sustituido por `xdg_shell`

En Wayland Explorer, se encontró información sobre [`xdg_shell`](https://wayland.app/protocols/xdg-shell)

Del video de YouTube tomado como referencia [^main_video] usamos:
```
wayland-scanner private-code /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml xdg-shell-protocol.c
wayland-scanner client-header /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml xdg-shell-client-protocol.h
gcc main.c xdg-shell-protocol.c -l wayland-client -o main
```

Al final, se notó que en video se estaban usando muchas instrucciones que se salían un poco
de lo primordial, por lo cual en el Wayland Book [^book], revisamos la sección de [XDG shell basics](https://wayland-book.com/xdg-shell-basics.html)
y se tomó el ejemplo de código de la sección [Extended example code](https://wayland-book.com/xdg-shell-basics/example-code.html)
como base. Este crea una ventana con un patrón de tablero.

Importante notar que luego, en Extended example code se dieron los comandos usados en el video, pero un poco diferentes
```
wayland-scanner private-code \
  < /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
  > xdg-shell-protocol.c
wayland-scanner client-header \
  < /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
  > xdg-shell-client-protocol.h
cc -o client client.c xdg-shell-protocol.c -lwayland-client -lrt
```
> [!NOTE]
> Se decidió usar los comandos del video

# Referencias consultadas

[^book]: [Wayland book](https://wayland.freedesktop.org/docs/book/)

[^protocol]: [The Wayland Protocol](https://wayland.freedesktop.org/docs/html/index.html)

[xdg_shell](https://wayland.app/protocols/xdg-shell)

[^tuto]: [Writing Wayland Clients](https://bugaevc.gitbooks.io/writing-wayland-clients/content/)

[^main_video]: [zero to window - wayland client (YouTube)](https://youtu.be/iIVIu7YRdY0?si=T4jjiITBwp_5KDXr)

[Criterios de aceptación: ejemplos para elaborarlos](https://www.scrummanager.com/blog/2023/03/criterios-de-aceptacion-definicion-y-ejemplos/)

[Sintaxis básica de redacción y formato](https://docs.github.com/es/get-started/writing-on-github/getting-started-with-writing-and-formatting-on-github/basic-writing-and-formatting-syntax)


