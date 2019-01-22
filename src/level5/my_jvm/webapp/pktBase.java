import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.lang.reflect.Modifier;
import java.lang.reflect.Type;

/**
 * 
 */

/**
 * @author user1
 *
 */
public class pktBase {
	public int msgLength = 0 ;
	
	
	public static final byte[] int2bytes(int val) {
		
		int size = 4;
		byte[] array = new byte[size];

		
		for (int i=0, pos=0, shift=0;i<size;i++,shift+=8)
			array[pos++] = (byte) ((val>>shift)&0xff) ;
		
		return array ;
	}
	
	
	public static final byte[] long2bytes(long val) {
		
		int size = 8;
		byte[] array = new byte[size];
		
		
		for (int i=0, pos=0, shift=0;i<size;i++,shift+=8) {
			array[pos++] = (byte) ((val>>shift)&0xff) ;
			//System.out.printf("save byte 0x%x\n",array[pos-1]&0xff);
		}
		
		return array ;
	}
	
	
	public static final int bytes2int(byte array[], int pos) {
		int val = 0;
		int size = 4;
		
		
		for (int i=0,shift=0;i<size;i++,shift+=8)
			val |= (array[pos++]<<shift) ;
		
		return val ;
	}	
	
	public static final long bytes2long(byte array[], int pos) {
		int size = 8;
		long val = 0;
		
		
		for (int i=0,shift=0;i<size;i++,shift+=8) {
			val &= ~(0xff << shift) ;
			val |= (array[pos++]<<shift) ;
			//System.out.printf("restore: 0x%x, val: 0x%x\n",array[pos-1],val);
		}
		
		return val ;
	}
	
	boolean isModifierOk(Field fld) {
		
		if ((fld.getModifiers() & Modifier.FINAL)!=0 || 
			(fld.getModifiers() & Modifier.STATIC)!=0) {
			System.out.printf("member '%s' 's modifier is '%d', NOT support\n",fld.getName(),fld.getModifiers());
			return false ;
		}
		return true ;
	}
	
	public int toObject(byte array[]) throws Exception {
		int pos = 0;
		Class cls = this.getClass() ;		
		Field[] fields = cls.getDeclaredFields() ;
		
		for (Field fld : fields) {
			Type t = fld.getGenericType() ;
			
			
			fld.setAccessible(true);
			
			if (!isModifierOk(fld)) {
				continue ;
			}

			switch (t.toString()) {
			case "byte" :
				fld.set(this, array[pos++]);
				break ;
			case "int" :
				fld.set(this,pktBase.bytes2int(array, pos));
				pos += 4;
				break ;
			case "long":
				fld.set(this,pktBase.bytes2long(array, pos));
				pos += 8;
				break ;
			case "class [B" :
				byte[] tmp = new byte[msgLength];
				System.arraycopy(array, pos, tmp, 0, msgLength);
			    pos += msgLength ;
			    System.out.println("msg len: " + msgLength);
			    
				fld.set(this,tmp);

				break ;
				
			default:
				System.out.printf("un-support field type '%s', name %s\n",t.toString(),fld.getName());
			}

		}
		
		return 0;
	}
	

	
	public final byte[] toBytes() throws Exception {
		int pos = 0;
		Class cls = this.getClass() ;		
		Field[] fields = cls.getDeclaredFields() ;
		byte[] array = new byte[getTotalSize()];

		
		for (Field fld : fields) {
			Type t = fld.getGenericType() ;
			
			
			fld.setAccessible(true);
			
			
			if (!isModifierOk(fld)) {
				continue ;
			}

			switch (t.toString()) {
			case "byte" :
				array[pos++] = (byte) fld.get(this) ;
				break ;
			case "int" :
				System.arraycopy(pktBase.int2bytes((int)fld.get(this)), 0, array, pos, 4);
				pos += 4 ;
				break ;
			case "long":
				System.arraycopy(pktBase.long2bytes((long)fld.get(this)), 0, array, pos, 8);
				pos += 8;
				break ;
			case "class [B" :
				byte[] ba = ((byte[])fld.get(this));
				int mlen = ba.length<msgLength? ba.length : msgLength ;
				int rest = msgLength-mlen;
				
				
				System.arraycopy(ba, 0, array, pos, mlen);
			    pos += mlen ;
			
				for (int i=0;i<rest;i++)
					array[pos++] = 0 ;

				break ;
				
			default:
				System.out.printf("un-support field type '%s', name %s\n",t.toString(),fld.getName());
			}

		}
			   
		return array;
	}
	
	
	public final int getTotalSize() {
		int size = 0;
		Class cls = this.getClass() ;		
		Field[] fields = cls.getDeclaredFields() ;
		
		for (Field fld : fields) {
			Type t = fld.getGenericType() ;
			
			
			fld.setAccessible(true);
			
			if (!isModifierOk(fld)) {
				continue ;
			}

			switch (t.toString()) {
			case "byte" :
				size += 1;
				break ;
			case "int" :
				size += 4;
				break ;
			case "long":
				size += 8;
				break ;
			case "class [B" :
				size += msgLength;
				break ;
				
			default:
				System.out.printf("un-support field type '%s', name %s\n",t.toString(),fld.getName());
			}

		}
		
		System.out.println("total size is " + size);
		
		return size ;
	}
	

	public final void dump() throws Exception {
		Class cls = this.getClass() ;		
		Field[] fields = cls.getDeclaredFields() ;
		
		for (Field fld : fields) {
			Type t = fld.getGenericType() ;
			
			
			fld.setAccessible(true);

			switch (t.toString()) {
			case "byte" :
				System.out.printf("member '%s' = %d\n",fld.getName(),(byte) fld.get(this));
				break ;
			case "int" :
				System.out.printf("member '%s' = %d\n",fld.getName(),(int) fld.get(this));
				break ;
			case "long":
				System.out.printf("member '%s' = %d\n",fld.getName(),(long) fld.get(this));
				break ;
			case "class [B" :
				byte[] ba = (byte[]) fld.get(this);
				String bm = new String(ba) ;
				System.out.printf("member '%s' = %s\n",fld.getName(),bm);
				break ;
				
			default:
				System.out.printf("un-support field type '%s', name %s\n",t.toString(),fld.getName());
			}

		}
		
	}

}
