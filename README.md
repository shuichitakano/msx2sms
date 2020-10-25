# MSX2SMS

MSX1 の ROM を SMS 形式に変換します。

---
### 使用例

```
./bios_patcher -b msx.rom -p bios_patch.bin -m konami8k -r input.rom -o output.sms --stkey f1 --stkey f5
```

### オプション 

* -o : 出力ファイル名を指定します
* -r : 変換元のMSX ROMイメージを指定します
* -b : MSX の bios ROM を指定します
* -p : bios_patch.bin を指定します
* -m : マッパーの種類を指定します
  * ascii8k
  * ascii16k
  * konami8k
  * scc
* -t : PSG の周波数書き込み順を逆にします
* --stkey : start ボタンにアサインするキーを指定します。
  * ２つまで指定できます
  * 両方同時に押されたことになります
  
### 制限

* 言うまでもなく動かないものが大多数だと思います。
* 16KBのバンクで8Kのバンク切り替えを再現するために、適当にバンクのペアを解析して生成するため、変換後のイメージは元のMSXのROMよりもかなり大きくなることがあります。
* バンクテーブル探索などのため、MSX実機より遅くなることがあります。
* 低周波の音階が足りないため、低域で音程が1オクターブ上がることがありますが、仕様です。
* たぶん bios ROM は日本版しか使えないと思います。

### 備考
* hexファイルをバイナリにするために、devkitSMS の ihx2sms を使用しています。
