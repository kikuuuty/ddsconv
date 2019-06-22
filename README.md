# ddsconv

JPEG,PNG,BMP及びTGAファイルなどを読み込み、DDSファイルとして出力します。  
圧縮フォーマットはBC1,BC3,BC5,BC6H,BC7が選択可能です。  
BC1/3およびBC6H/BC7の圧縮には[ISPC Texture Compressor](https://github.com/GameTechDev/ISPCTextureCompressor)を使用しているため、非常に高速かつ高品質な圧縮が行えます。

## ビルド
ISPCのバイナリを別途ダウンロードする必要があります。  
https://ispc.github.io/downloads.html

Windows向けのバイナリをダウンロードして、次のディレクトリに配置してください。  

- ISPC/win/ispc.exe

DirectXTexをddsconvと同じディレクトリにクローンする必要があります。  
https://github.com/microsoft/DirectXTex

```
+-- your-project-dirs/
    +-- ddsconv/
    +-- DirectXTex/
```

## 使い方
詳しい使い方は、-hまたは--helpオプションを参照してください。  
