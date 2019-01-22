import java.io.InputStream;
import java.io.OutputStream;
import java.net.InetAddress;
import java.net.Socket;


public class client {

    /**
     * @param args
     */
    public static void main(String[] args) {
        // TODO Auto-generated method stub
        try {
        	Request req = new Request();
			Socket s=new Socket(InetAddress.getLoopbackAddress(),8881);
            OutputStream ostr = s.getOutputStream();

            String msg = "test messages to server";
            

            req.setLen(19);
            req.setMagic((byte) 8);
            req.setMsg(msg.getBytes());
            req.setShortVer((byte) 66);
            req.setVersion(271);
            req.setTime(System.currentTimeMillis());
            
            ostr.write(req.toBytes());
            
            
            InputStream istr = s.getInputStream() ;
            Response resp = new Response();
            byte array[] = new byte[resp.getTotalSize()]; 
            
            // receive a reponse
            for (int pos = 0, total = resp.getTotalSize(),ret=0; 
                 (ret=istr.read(array,pos,total))!=-1 && total>0; pos+=ret,total-=ret) ;

            resp.toObject(array) ;
            resp.dump();

        }catch (Exception e) {
                // TODO Auto-generated catch block
                e.printStackTrace();
        }

    }
}
